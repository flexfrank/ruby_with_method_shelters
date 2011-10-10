shelter :A do
  def a(x)
    x+1
  end
end

shelter :B do
  import :A
  def x
    p (1..10).map{|i| a(i)}
  end
end
shelter_eval :B do
  x
end
