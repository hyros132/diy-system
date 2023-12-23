#### 计算机启动流程简介



![image-20231220220140706](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220220140706.png)

#### 创建可引导的启动程序

开发流程

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220220353764.png" alt="image-20231220220353764" style="zoom:33%;" />

#### 初始化引导程序

我们设计的系统三个步骤（不同操作系统不一样）

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220220601791.png" alt="image-20231220220601791" style="zoom: 33%;" />

BIOS只加载磁盘的第0个扇区到内存中，此部分程序无法做很多事情

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220220821447.png" alt="image-20231220220821447" style="zoom:33%;" />

因此，有两种方式，我们采取的是第二种

![image-20231220220959818](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220220959818.png)

这些是段寄存器，为访问特定的内存地址，需要 采用段：偏移 的形式。但是我们设置寄存器的偏移量全为0

![image-20231220221208850](img/:Users:hyros:code:OS:mycode:doc:img::Users:hyros:Library:Application Support:typora-user-images:image-20231220221208850.png)

8086内存映射，实模式下，只能访问1MB

![image-20231220221543424](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220221543424.png)

#### 使用BIOS中断显示字符

BIOS提供了一组服务，可以方便地帮助我们操纵硬件，避免与硬件细节打交道

<img src="./img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220210822497.png" alt="image-20231220210822497" style="zoom: 33%;" />

中断向量表存储在0x0000 0000到0x 0000 03FF处，里面存储了不同中断处理函数的入口地址

当出发软中断时，会自动从中断向量表中去相应的地址执行，参数通过寄存器传递

<img src="./img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220211102609.png" alt="image-20231220211102609" style="zoom: 33%;" />

*实战：*

```systemverilog
//像寄存器传递参数
mov $0xe, %ah
mov $'L', %al
//触发软中断
int $0x10
```

效果：qemu启动后成功显示字符 L

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220212242939.png" alt="image-20231220212242939" style="zoom:33%;" />

#### 使用BIOS中断程序读取磁盘

BIOS提供软磁盘读取多接口，方便我们从磁盘上读取loader

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220215203196.png" alt="image-20231220215203196" style="zoom:33%;" />

我们的loader位于boot后面的扇区（后文假设读取64个扇区），然后利用中断程序将这64个扇区读取到内存0x8000处（这个地址是自定义的）

![image-20231220215343581](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220215343581.png)

INT13指令解释：

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220212910842.png" alt="image-20231220212910842" style="zoom:33%;" />

实战代码：

```
read_loader:
	// 读到内存的0x0080处
	mov $0x8000, %bx
	mov $0x2, %ah
	mov $0x2, %cx
	// 读取64个扇区
	mov $64, %al
	mov $0x0080, %dx
	int $0x13
	jc read_loader
```

disk.img第2个扇区（每个扇区512B，第二个扇区的起始地址为0x00000200）的前32个字节数据

![image-20231220214745043](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220214745043.png)

代码执行完毕，我们在vscode的debug console查看内存地址0x8000的钱20个字节的数据，发现和磁盘文件disk.img中是一致的

![image-20231220215015029](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231220215015029.png)

#### 进入c语言并跳转到loader

在boot中添加

```
	//跳转到c代码
	jmp boot_entry
	jmp .
```

boot.c文件

![image-20231221192412677](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231221192412677.png)

为了让编译器知道0x8000处是一个函数，还需要在lau ch.json中添加

![image-20231221192601408](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231221192601408.png)

在script中吧对应的磁盘写入文件的注视打开，以便于把代码写入道磁盘中（方便qemu读取）

![image-20231221200911320](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231221200911320.png)

#### 利用内联汇编显示字符串

在loader_16.c下添加show_msg函数

```c
static void show_msg(const char* msg){
    char c;
    while((c=*msg++)!='\0'){
        __asm__ __volatile__(
            "mov $0xe, %%ah\n\t"
            "mov %[ch], %%al\n\t"
            "int $0x10"::[ch]"r"(c)
        );
    }
}
```

#### 检测内存容量

在comm目录下新建boot_info.h和types.h头文件，添加相关信息

在loader.h中添加SMAP_entry结构体

在loader_16.c中添加detect_memory函数

```c
// 参考：https://wiki.osdev.org/Memory_Map_(x86)
// 1MB以下比较标准, 在1M以上会有差别
// 检测：https://wiki.osdev.org/Detecting_Memory_(x86)#BIOS_Function:_INT_0x15.2C_AH_.3D_0xC7
static void  detect_memory(void) {
	uint32_t contID = 0;
	SMAP_entry_t smap_entry;
	int signature, bytes;

    show_msg("try to detect memory:");

	// 初次：EDX=0x534D4150,EAX=0xE820,ECX=24,INT 0x15, EBX=0（初次）
	// 后续：EAX=0xE820,ECX=24,
	// 结束判断：EBX=0
	boot_info.ram_region_count = 0;
	for (int i = 0; i < BOOT_RAM_REGION_MAX; i++) {
		SMAP_entry_t * entry = &smap_entry;

		__asm__ __volatile__("int  $0x15"
			: "=a"(signature), "=c"(bytes), "=b"(contID)
			: "a"(0xE820), "b"(contID), "c"(24), "d"(0x534D4150), "D"(entry));
		if (signature != 0x534D4150) {
            show_msg("failed.\r\n");
			return;
		}

		// todo: 20字节
		if (bytes > 20 && (entry->ACPI & 0x0001) == 0){
			continue;
		}

        // 保存RAM信息，只取32位，空间有限无需考虑更大容量的情况
        if (entry->Type == 1) {
            boot_info.ram_region_cfg[boot_info.ram_region_count].start = entry->BaseL;
            boot_info.ram_region_cfg[boot_info.ram_region_count].size = entry->LengthL;
            boot_info.ram_region_count++;
        }

		if (contID == 0) {
			break;
		}
	}
    show_msg("ok.\r\n");
}
```

#### 切换保护模式

cpu上电复位后默认进入实模式，在这种模式下没有保护机制，但提供了BIOS服务 

从实模式切换至保护模式需要遵循一定的流程

![image-20231223094439325](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231223094439325.png)

参考资料：

- 实模式：https://wiki.osdev.org/Real_Mode
- A20地址线：https://blog.csdn.net/sinolover/article/details/93877845

<img src="img/:Users:hyros:Library:Application Support:typora-user-images:image-20231223094900242.png" alt="image-20231223094900242" style="zoom:33%;" />

步骤：

- 关中断

- 打开A20地址线
- 加载GDT表

检查GDT表是否成功加载

在qemu中查看寄存器状态，info registers，可以看到GDTR寄存器中的数据，前32位是gdt表在内存中的地址，后16位是gdt表的大小（24的十六进制-1）因为是索引值，所以减1。

当前gdt表的内容：

```c
uint16_t gdt_table[][4] = {
    {0, 0, 0, 0},
    {0xFFFF, 0x0000, 0x9A00, 0x00CF},
    {0xFFFF, 0x0000, 0x9200, 0x00CF},
};
```

大小是3*8=24B。

![image-20231223112357180](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231223112357180.png)

可以看到vscode的调试信息中gdt_table的地址是0x9518，与GDT寄存器保持一致

![image-20231223113117888](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231223113117888.png)

- 设置CR0

![image-20231223101031002](img/:Users:hyros:Library:Application Support:typora-user-images:image-20231223101031002.png)

- 远跳转

以上流程的代码实现代码实现 loader_16.c

```c
//进入保护模式
static void enter_protect_mode(void){
  //关中断
	cli();
	//开启A20地址线
	uint8_t v = inb(0x92);
	outb(0x92, v|0x2);
	//加载gdt表
	lgdt((uint32_t)gdt_table,sizeof(gdt_table));
	//设置CR0寄存器
 	uint32_t cr0=read_cr0();
	write_cr0(cr0|(1<<0));
	//远跳转
	far_jump(8,(uint32_t)protect_mode_entry);
}
```

#### 实用LBA模式读取磁盘

