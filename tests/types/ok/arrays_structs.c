// type-correct: array indexing and struct field access
struct Vec {
	int x;
	int y;
};

int sum(int v[], int n)
{
	int i;
	int s;
	s = 0;
	for (i = 0; i < n; i = i + 1) s = s + v[i];
	return s;
}

int main()
{
	int v[10];
	struct Vec a;
	int i;

	i = 0;
	v[i] = 5;             // index with an int, store an int
	a.x = v[0];           // field assignment
	a.y = a.x + 1;
	return a.x + a.y;
}
