#include <stddef.h>
#include <stdint.h>
#include "Drive.h"
#include "Info.h"

// FIXME: refactor with BIT macros
#define IS_FLAG_SET(flags, flag) \
    (((flags) & (flag)) != 0)

extern "C" multiboot_info* e_multiboot_info;
extern "C" uint32_t e_multiboot_magic_nr;

class Memory_Region_Iterator {
public:
    Memory_Region_Iterator(multiboot_mmap_entry* array, size_t size_bytes);
    bool has_next();
    multiboot_mmap_entry* next();

private:
    multiboot_mmap_entry* _array;
    size_t _position;
    size_t _size_bytes;
};

E_BIT_ATTR_PACKED(struct Boot_Device {
    // Partition numbers start at zero.
    enum {BOOT_DEVICE_UNUSED_PARTITION = 0xFF};

    uint8_t sub_sub_partition;
    uint8_t sub_partition;
    uint8_t top_level_partition;
    e_Multiboot_Drive_BIOS_Nr drive_number;
});

Memory_Region_Iterator::Memory_Region_Iterator(
    multiboot_mmap_entry* array, size_t size_bytes) :
    _array(array), _position(0), _size_bytes(size_bytes) {
}

bool Memory_Region_Iterator::has_next() {
    return this->_position < this->_size_bytes;
}

multiboot_mmap_entry* Memory_Region_Iterator::next() {
    uint8_t* address = reinterpret_cast<uint8_t*>(_array) + _position;
    multiboot_mmap_entry* region
        = reinterpret_cast<multiboot_mmap_entry*>(address);

    _position += sizeof(region->size) + region->size;
    return region;
}

static void log_boot_device(struct multiboot_info* info, struct e_Log* log) {
    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_BOOTDEV)) {
        Boot_Device& device = reinterpret_cast<Boot_Device&>(info->boot_device);

        e_Log_msg(log, E_LOG_INFO,
            "Boot device: drive={iuh}; partitions=[{iuh}, {iuh}, {iuh}]",
            device.drive_number,
            device.top_level_partition,
            device.sub_partition,
            device.sub_sub_partition);
    }
}

static void log_boot_modules(struct multiboot_info* info, struct e_Log* log) {
    if (!IS_FLAG_SET(info->flags, MULTIBOOT_INFO_MODS)) {
        return;
    }

    for (size_t i = 0; i < info->mods_count; ++i) {
        multiboot_mod_list& module = reinterpret_cast<multiboot_module_t*>(
            info->mods_addr)[i];

        e_Log_msg(log, E_LOG_INFO,
            "Boot module: start={iuh}; end={iuh}; string=\"{s}\"",
            module.mod_start, module.mod_end,
            reinterpret_cast<char*>(module.cmdline));
    }
}

static void log_drives(struct multiboot_info* info, struct e_Log* log) {
    if (!IS_FLAG_SET(info->flags, MULTIBOOT_INFO_DRIVE_INFO)) {
        return;
    }

    e_Multiboot_Drive_Iterator iterator(
        reinterpret_cast<e_Multiboot_Drive*>(info->drives_addr),
        info->drives_length);

    while (iterator.has_next()) {
        iterator.next()->log();
    }
}

static void log_memory_map(struct multiboot_info* info, struct e_Log* log) {
    if (!IS_FLAG_SET(info->flags, MULTIBOOT_INFO_MEM_MAP)) {
        return;
    }

    Memory_Region_Iterator iterator(
        reinterpret_cast<multiboot_mmap_entry*>(info->mmap_addr),
        info->mmap_length);

    // The memory map is provided by the BIOS and is guaranteed to list
    // all standard memory that should be available for normal use.

    while (iterator.has_next()) {
        multiboot_mmap_entry* region = iterator.next();

        e_Log_msg(log, E_LOG_INFO, "Memory map region: addr=[{iuh}, {iuh}]; "
            "size=[{iuh}, {iuh}] B; type={iu}",
            static_cast<unsigned>(region->addr >> 32),
            static_cast<unsigned>(region->addr & 0xFFFFFFFF),
            static_cast<unsigned>(region->len >> 32),
            static_cast<unsigned>(region->len & 0xFFFFFFFF),
            region->type);
    }
}

static void log_symbol_table(struct multiboot_info* info, struct e_Log* log) {
    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_AOUT_SYMS)) {
        multiboot_aout_symbol_table& table = info->u.aout_sym;

        e_Log_msg(log, E_LOG_INFO, "a.out symbol table: "
            "size={iu}; string table size={iu}; addr={iuh}",
            table.tabsize, table.strsize, table.addr);
    }
    else if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_ELF_SHDR)) {
        multiboot_elf_section_header_table& table = info->u.elf_sec;

        e_Log_msg(log, E_LOG_INFO, "ELF section header table: "
            "num={iu}; size={iu} B; addr={iuh}; shndx={iu}",
            table.num, table.size, table.addr, table.shndx);
    }
}

static void log_vbe(struct multiboot_info* info, struct e_Log* log) {
    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_VBE_INFO)) {
        e_Log_msg(log, E_LOG_INFO, "VBE: control info={iuh}; mode info={iuh}; "
            "current video mode={iu}",
            info->vbe_control_info,     // Obtained by VBE function 00h.
            info->vbe_mode_info,        // Obtained by VBE function 01h.
            info->vbe_mode);            // In VBE 3.0 format.
    }

    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_FRAMEBUFFER_INFO)) {
        /* The following fields contain the table of a protected mode
           interface defined in VBE version 2.0 or later. If it isn't
           available, those fields are set to zero. */

        e_Log_msg(log, E_LOG_INFO, "VBE 2.0+ interface table: segment={iuh}; "
            "offset={iuh}; length={iu} B", info->vbe_interface_seg,
            info->vbe_interface_off, info->vbe_interface_len);
    }
}

struct multiboot_info* e_Multiboot_Info_get() {
    if (e_multiboot_magic_nr != MULTIBOOT_BOOTLOADER_MAGIC) {
        e_Log_msg(e_Log_get(), E_LOG_ERROR,
            "Invalid Multiboot magic number: {iuh}", e_multiboot_magic_nr);
    }

    if (IS_FLAG_SET(e_multiboot_info->flags, MULTIBOOT_INFO_AOUT_SYMS)
        && IS_FLAG_SET(e_multiboot_info->flags, MULTIBOOT_INFO_ELF_SHDR))
    {
        e_Log_msg(e_Log_get(), E_LOG_ERROR,
            "Invalid Multiboot information: "
                "Both bits 4 and 5 of the flags field are set.");
    }

    return e_multiboot_info;
}

void e_Multiboot_Info_log(struct multiboot_info* info, struct e_Log* log) {
    e_Log_msg(log, E_LOG_INFO, "Multiboot: flags={iub}", info->flags);

    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_MEMORY)) {
        e_Log_msg(log, E_LOG_INFO, "Memory: lower={iu} KiB; upper={iu} KiB",
            info->mem_lower,        // From 0 to 640.
            info->mem_upper);       // Starting at 1024.
    }

    log_boot_device(info, log);

    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_CMDLINE)) {
        e_Log_msg(log, E_LOG_INFO, "Command line: \"{s}\"", info->cmdline);
    }

    log_boot_modules(info, log);
    log_symbol_table(info, log);
    log_memory_map(info, log);
    log_drives(info, log);

    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_CONFIG_TABLE)) {
        /* ROM configuration table as returned by the "get configuration"
           BIOS call. If it failed, the size of the table must be zero. */

        e_Log_msg(log, E_LOG_INFO, "ROM configuration: table={iuh}",
            info->config_table);
    }

    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_BOOT_LOADER_NAME)) {
        e_Log_msg(log, E_LOG_INFO, "Boot loader: \"{s}\"",
            info->boot_loader_name);
    }

    if (IS_FLAG_SET(info->flags, MULTIBOOT_INFO_APM_TABLE)) {
        multiboot_apm_info& table = *reinterpret_cast<multiboot_apm_info*>(
            info->apm_table);

        e_Log_msg(log, E_LOG_INFO, "APM table: version={iu}; flags={iub}",
            table.version, table.flags);
    }

    log_vbe(info, log);
}
