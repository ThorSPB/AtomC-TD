// type error: the if condition must be a scalar value
// a struct cannot be used as a truth value
struct Pt { int x; };

void main()
{
	struct Pt p;
	if (p) put_i(0);
}
