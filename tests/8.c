// lexical analyzer test
void main()
{
	if(0xc==014)put_s("\"equal\"\t\t(h,o)");
		else put_s("\"not equal'\"\t\t(h,o)");
	if(20E-1==2.0&&0.2e+1==0x2)put_c('=');  // 2 written in various ways
		else put_c('\\');
}
