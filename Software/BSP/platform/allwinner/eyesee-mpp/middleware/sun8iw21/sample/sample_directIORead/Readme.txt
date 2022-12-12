sample_directIORead：
    该sample演示使用directIO方式读文件。理论上不占内存作为缓存。

读取测试参数的流程：
    sample支持从命令行读入配置文件路径，解析配置文件。如果没有配置文件，使用默认参数进行测试。
    从命令行启动sample_directIORead的指令：
    ./sample_directIORead -path sample_directIORead.conf

测试参数的说明：
(1)src_file：指定被读取的源文件。
(2)dst_file：读出的内容写入目标文件，可以在测试结束后和源文件比对。判断是否读取正确。如果为空，则读取内容不写入目标文件。
(3)read_size：指定一次读取的长度，单位字节
(4)read_interval：指定读取的间隔时间，单位：ms
(5)loop_cnt：指定循环读取文件的次数，0表示无限次
