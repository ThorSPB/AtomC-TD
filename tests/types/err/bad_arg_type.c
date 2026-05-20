// type error: argument 1 of put_i has an incompatible type
// a struct value cannot be passed where an int is expected
struct Pt { int x; };

void main()
{
	struct Pt p;
	put_i(p);
}
