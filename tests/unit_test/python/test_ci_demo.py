from ci_demo import example_a 

# ci_demo
def test_example_a():
    assert example_a("Try Me") == ("others")
    assert example_a("case 1") == ("case 1")
    assert example_a() == ("None")

