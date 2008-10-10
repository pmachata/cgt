#include "rand.hh"
#include <boost/random/linear_congruential.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/random/variate_generator.hpp>

namespace {
  typedef boost::minstd_rand base_generator_type;
  base_generator_type generator(42u);
  boost::uniform_real<> uni_dist(0,1);
  boost::variate_generator<base_generator_type&, boost::uniform_real<> > uni(generator, uni_dist);
}

double
cg::rand()
{
  return uni();
}
