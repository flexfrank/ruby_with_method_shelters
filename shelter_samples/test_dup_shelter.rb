shelter :A do
  class A
    def a
      1
    end
  end
end
shelter :A do
  class A
    def a
      2
    end
  end
end

shelter_eval :A do
p A.new.a
end
