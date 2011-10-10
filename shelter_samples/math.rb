shelter :MathN do
  class Fixnum
    def /(other)
      Rational(self,other)
    end
  end
end

shelter :UseMathN do
  def inverse(n)
    return 1/n
  end
hide
  import :MathN
end

shelter :Client do
  import :UseMathN
end

p(1/2) # prints "0"
shelter_eval :Client do
  p(1/2) # prints "(1/2)"
  p(inverse(2))
end
