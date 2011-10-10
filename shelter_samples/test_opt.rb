shelter(:A) do
  class String
    def size
      -1
    end
  end
end
shelter :B do
  import :A
  class Fixnum
    def +(o)
      self*o
    end
  end
end

p 2+4
p "aaaa".size
shelter_eval :B do
  p 2+4
  p "aaaa".size
end
