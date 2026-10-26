ENTRY(__start)
SECTIONS { .text 0x1000 : { *(.text) } .data 0x10000 : { *(.data) *(.bss) } .table 0x0101e5e8 : { *(.table) } }
