#ifndef LPCONST_H
#define LPCONST_H
#define ID_NAME_MAX 64
#endif //LPCONST_H
abcdefghij_lmnopqrstuv_xyzabc_efghijklm_opqrstuvwxyz_bcdefg_ijkl
����һ�������޷����ܵ�̫�������ƹ��Ų�һ�����ֳ������ŵ�����

struct {...}
binary {...}
regex  {...}
A      {...}
a      {...}
{a,b,c}




case foo() of
    _ regex {'^good *', m 'morn*ing', '.*$'}:
        print("i get morning as $m\n");
    _ struct {_ = 5, b string, c float}:
        print("b = ${b: -15}, c = $c\n");
    {5, _, a}:
        print("a = $a\n");
    r'^.*$':
end
    
