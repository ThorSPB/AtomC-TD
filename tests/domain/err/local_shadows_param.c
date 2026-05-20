// domain error: symbol redefinition: x
// the function body reuses the parameter scope, so a local variable
// cannot share a name with a parameter (a nested { } block could)
int f(int x)
{
	int x;
	return x;
}
