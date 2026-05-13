// exercises every expression precedence level + casts + unary + chained assign
int main()
{
	int a;
	int b;
	int c;
	double d;

	// chained assignment (right-associative)
	a = b = c = 5;

	// full precedence ladder
	a = -b + c * 2 / (b - 1) > 0 && b != 0 || c == c;

	// cast (LPAR typeBase RPAR exprCast)
	d = (double) a;
	a = (int) d + 1;

	// unary chain
	a = - - !b;

	// nested calls + postfix index/dot would need a struct; expressions only here
	a = (a + b) * (c - 1);

	return a;
}
