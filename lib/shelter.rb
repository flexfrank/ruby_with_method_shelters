class Module
  def shelter_accessor(name)
    define_method name do
      ivname=Shelter.iv_name(name)
      self.instance_variable_get(ivname)
    end
    define_method (name.to_s+"=").to_sym do|val|
      ivname= Shelter.iv_name(name)
      self.instance_variable_set(ivname,val)
    end
  end
end
