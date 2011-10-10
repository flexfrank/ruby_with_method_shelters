shelter :A do
  p [:before,current_shelter]
  load "test2.rb"
  p [:after,current_shelter]
end
