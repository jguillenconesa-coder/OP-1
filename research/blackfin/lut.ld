ENTRY(__start)
SECTIONS { .text 0x1000 : { *(.text) } .data 0x10000 : { *(.data) } .source 0x018b6688 : { *(.source) } }
