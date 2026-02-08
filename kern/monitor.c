// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

int mon_show(int argc, char **argv, struct Trapframe *tf);

// LAB 1: add your command to here...
static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display a stack backtrace", mon_backtrace },
	{ "hidden", "Run hidden test cases", exec_hidden_cases},
	{ "show", "Print colorful ASCII art", mon_show },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t ebp = read_ebp();

	cprintf("Stack backtrace:\n");

	while (ebp != 0) {
		uint32_t *frame = (uint32_t *) ebp;
		uint32_t eip = frame[1];

		cprintf("  ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
			ebp, eip, frame[2], frame[3], frame[4], frame[5], frame[6]);

		struct Eipdebuginfo info;
		debuginfo_eip(eip - 1, &info);  
		cprintf("         %s:%d: %.*s+%d\n",
			info.eip_file,
			info.eip_line,
			info.eip_fn_namelen,
			info.eip_fn_name,
			(eip - 1) - info.eip_fn_addr);

		ebp = frame[0];
	}

	return 0;
}


static void
cprint(uint8_t fg, const char *s)
{
    console_setcolor(fg, 0);   // background = black
    cprintf("%s", s);
}

int
mon_show(int argc, char **argv, struct Trapframe *tf)
{
    cprintf("\n");

    // 6-color banner
    cprint(12, "  #######  ");   
    cprint(14, "#######  ");    
    cprint(10, "#######  ");    
    cprint(11, "#######  ");     
    cprint(9,  "#######  ");     
    cprint(13, "#######\n");    

    // ASCII cat art
    cprint(14, "          /\\_/\\\n");
    cprint(10, "         ( o.o )\n");
    cprint(11, "          > ^ <\n");

    // restore default color 
    console_setcolor(7, 0);
    cprintf("\n");
    return 0;
}

int exec_hidden_cases(int argc, char **argv, struct Trapframe *tf) {
	hidden_test_cases();
	return 0;
}

/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
