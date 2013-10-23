lp
    puts "hello world from lp $$"
end

fun foo(i)
    puts "$i: hello world from function foo in lp $$"
end

for i in 1..5 do
    p = lp foo(i) end
    puts "lp ${p.id} created"
end

while line in gets(); line != nil do
    puts line
end

i=1
while i < 2 do
    puts "dead loop $i"  # i = 1
    i = i + 1   # i = 2
end

puts i   # 1

do
    i = i+5
    puts i   # 6
end

puts i    # 1
