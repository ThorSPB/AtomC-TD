// type error: a field can only be selected from a struct
void main()
{
	int x;
	x = x.foo;
}
