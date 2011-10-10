def x(b)
  [current_node,b.respond_to?(:b)]
end

$defA = lambda do
shelter :A do
  class A
    def a
    end
  end
end
end
$defA[]
shelter :B do
  class B
    def b
      p x(self)
    end
  end
  hide
  import :A
end
2.times do
shelter_eval :B do
  p A.new.respond_to?(:a)
  p B.new.respond_to?(:b)
  B.new.b
end
$defA[]
end
  p A.new.respond_to?(:a)
  p B.new.respond_to?(:b)
