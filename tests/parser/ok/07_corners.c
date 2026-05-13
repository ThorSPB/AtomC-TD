// corner cases not covered by the other parser tests:
//   - empty statement `;`
//   - `return;` with no expression
//   - `for(;;)` with all three parts optional + empty body
//   - LESSEQ (<=) and GREATEREQ (>=) operators
//   - if without else (also covered in 03 inside a for, included again here at top)
//   - empty struct (no fields)
//   - empty arrayDecl `[]` (no size)

struct Empty {
};

void noop()
{
	return;
}

int clamp(int x)
{
	if (x <= 0) return 0;
	if (x >= 100) return 100;
	return x;
}

void main()
{
	int v[];

	;                          // empty statement

	for (;;) ;                 // infinite loop with empty body (which is itself just `;`)

	if (1 <= 2) noop();        // if without else
}
