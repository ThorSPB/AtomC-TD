// function with parameters, calls with 0/1/N args, nested calls,
// and a parameter that is an array (fnParam with arrayDecl).
int add(int a, int b)
{
	return a + b;
}

int max(int a, int b)
{
	if (a > b) return a;
	return b;
}

// fnParam with arrayDecl — pass an array by reference (no size required)
int firstOf(int v[])
{
	return v[0];
}

void main()
{
	int x;
	x = add(1, 2);
	x = max(add(1, 2), add(3, 4));
	put_i(x);
}
