// struct definition + struct typeBase + array (with constant size only)
struct Pt {
	int x;
	int y;
};

struct Pt origin;
struct Pt points[20];

int main()
{
	origin.x = 0;
	origin.y = 0;
	points[0].x = 1;
	points[0].y = 2;
	return points[0].x + points[0].y;
}
