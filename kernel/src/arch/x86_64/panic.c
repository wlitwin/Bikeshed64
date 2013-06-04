
void panic(const char* message)
{
	__asm__("hlt");
}
