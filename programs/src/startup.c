
extern void main(void);

void _start(void)
{
	main();

	// TODO call exit

	while (1)
	{
		__asm__ volatile("hlt");
	}
}
