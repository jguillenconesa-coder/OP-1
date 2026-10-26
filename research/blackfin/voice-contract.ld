ENTRY(__start)
SECTIONS { .text 0x1000 : { *(.text) } .data 0x10000 : { *(.data) *(.bss) } .sdram 0x01000000 : { *(.sdram) } }
