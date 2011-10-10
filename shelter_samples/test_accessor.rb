require "shelter"
shelter :A do
  class A
    shelter_accessor :a
  end
end
shelter :B do
  import :A
end
a=A.new
shelter_eval :A do
  a.a=1
  p a.a
  shelter_eval :B do
    a.a=2
    p a.a
  end
  p a.a
end

shelter_eval :A do
 p a.a
end
