// valid scoping: an inner scope may reuse a name from an outer one
int x;               // a global

int f(int x)         // OK: a parameter may shadow the global
{
	{
		int x;       // OK: a nested block is a new domain
	}
	{
		int x;       // OK: a separate block, also fine
	}
	return x;
}

void main()
{
	int y;
	{
		int y;       // OK: shadows the function-body y
	}
}
