shelter :MathN do
  class Fixnum
    def /(x)
      Rational(self,x)
    end
  end
end
shelter :Average do
  class Array
    def avg
      sum = self.inject(0){|r,i|r+i}
      sum / self.size
    end
  end
  hide
  import :MathN
end

shelter :MathClient do
  import :Average
  
  def calc
    p([1,2,3,4,5,6,7,8,9,10].avg) # prints "(55/10)"
    p(55/10) # prints 5
  end
end

shelter_eval :MathClient do
  calc
end

