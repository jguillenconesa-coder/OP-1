ENTRY(__start)
SECTIONS { .text 0x1000 : { *(.text) } .data 0x8000 : { *(.data) *(.bss) } }
