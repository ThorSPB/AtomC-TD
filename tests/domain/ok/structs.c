// valid struct usage: a struct is defined before use, and used both as a
// variable type and as the type of another struct's field
struct Point {
	int x;
	int y;
};

struct Rect {
	struct Point topLeft;
	struct Point bottomRight;
};

struct Rect screen;

int area()
{
	struct Point p;
	return p.x;
}
