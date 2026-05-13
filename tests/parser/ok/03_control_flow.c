// if/else, while, for, break, return
int abs(int x)
{
	if (x < 0) return -x;
	else return x;
}

int sum(int n)
{
	int i;
	int s;
	s = 0;
	for (i = 0; i < n; i = i + 1) {
		if (i == 5) break;
		s = s + i;
	}
	return s;
}

void main()
{
	int x;
	x = 0;
	while (x < 10) x = x + 1;
}
