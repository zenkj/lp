#1 integer value definition

i0 = 10
i1 = i0 + 5
i2 = i0 - 5
i3 = i0 * 5
i4 = i0 / 5
i5 = i0 % 5
i6 = i0 ** 5
i7 = i0 >> 5
i8 = i0 << 5
i9 = 0123
ia = 0xabc
ic = 0b1001


#= float value definition

f0 = 1.5
f1 = f0 + 1.5
f2 = f0 - 1.5
f3 = f0 * 1.5
f4 = f0 / 1.5


#= boolean value definition
b0 = true
b1 = false
b2 = b0 && b1
b3 = b0 || b1
b4 = ! b0
b5 = b0 == b1
b5 = b0 != b1
b5 = f0 == f1
b5 = f0 > f1
b5 = f0 < f1
b5 = f0 >= f1
b5 = f0 <= f1
b5 = f0 != f1


#= character string(unicode string) value definition
s0 = 'abcde'
s1 = "abcde"
s2 = "ab$$cde"
s3 = "s0 = $s0"         # variable substitution
s4 = "s0 = ${f0 + 1.5}" # variable substitution
s5 = 's0 = $s0'         # NO variable substitution

#= bytestring value definition
# to be determined


#= regex value definition
r0 = /^.*$/
r1 = /^\/abc\/def\/.*$/
r2 = r'^/abc/def/.*$/' # support or not???
r4 = /abc.*$s0\s*/


#= tuple value definition
a,b,c = 1,2,3


#= object value(named tuple) definition
T1 = { a = 4, b = 'hello', c boolean }
obj0 = T1 { c=true }
obj1 = T1{}
obj2 = obj1{a=6}
#obj3.b = 'world'   # error
obj4 = {a = 4, b = 6}{}


#= list value definition
l0 = [1,2,3]
l1 = [a+1 for a in l0]
l2 = [a*2 for a in i0; a != 2]
l3 = [a + b for a in l0; b in l2; a != b]


#= function value definition
fun0 = fun(a int) return a + c end


#= lp value definition
lp0 = lp puts true end
lp1 = lp0


#= null value definition
n0 = null


#= anonymous value definition
a,_,c = 1,2,3

