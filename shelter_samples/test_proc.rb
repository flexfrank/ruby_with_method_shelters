shelter :A do
  class A
    def a
      :a_desu
    end
  end
  def x(a)
    p a.a
    proc{p current_node;a.a}
  end
end
shelter :B do
  def y(a)
    x(a)
  end
  hide
  import :A
end
shelter :C do
  import :B
  def z
    a=A.new
    p y(a)[]
  end
end
l=lambda{1}
l[]
shelter_eval :C do
  z
end
