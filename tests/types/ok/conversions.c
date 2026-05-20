// type-correct: implicit numeric conversions and explicit casts
void main()
{
	int i;
	double d;
	char c;

	i = c;            // char -> int
	d = i;            // int -> double
	c = d;            // double -> char
	d = i + c;        // mixed arithmetic, then assigned to double
	d = (double) i;   // explicit widening cast
	i = (int) d;      // explicit narrowing cast
}
