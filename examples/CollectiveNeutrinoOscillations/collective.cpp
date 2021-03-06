 /******************************************************************************
 *    This program is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published by      *
 *   the Free Software Foundation, either version 3 of the License, or         *
 *   (at your option) any later version.                                       *
 *                                                                             *
 *   This program is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 *   GNU General Public License for more details.                              *
 *                                                                             *
 *   You should have received a copy of the GNU General Public License         *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                             *   
 *   Authors:                                                                  *
 *      Carlos Arguelles (University of Wisconsin Madison)                     * 
 *         carguelles@icecube.wisc.edu                                         *
 *      Jordi Salvado (University of Wisconsin Madison)                        *
 *         jsalvado@icecube.wisc.edu                                           *
 *      Christopher Weaver (University of Wisconsin Madison)                   *
 *         chris.weaver@icecube.wisc.edu                                       *
 ******************************************************************************/


/*******************************************************************************
 * Example of the collective phenomena for neturinos,                          *
 * based on Georg G. Raffelt  Phys.Rev. D83 (2011) 105022.                     *
 * arXiv:1103.2891                                                             *
 ******************************************************************************/

#include "collective.h"
#include <iostream>

void progressbar(int percent, double mu){
  static int last=-1;
  if(percent==last)
    return;
  last=percent;
  std::string bar;
  
  for(int i = 0; i < 50; i++){
    if( i < (percent/2)){
      bar.replace(i,1,"=");
    }else if( i == (percent/2)){
      bar.replace(i,1,">");
    }else{
      bar.replace(i,1," ");
    }
  }
  
  std::cout<< "\r" "[" << bar << "] ";
  std::cout.width( 3 );
  std::cout<< percent << "%   mu: " << mu << std::flush;
}

using squids::SU_vector;

void collective::init(double m,double th, double wmin, double wmax, int Nbins){
  ini(Nbins/*nodes*/,2/*SU(2)*/,1/*density matrices*/,0/*scalars*/);
  
  mu=m;
  w_min=wmin;
  w_max=wmax;
  theta=th;

  Set_xrange(wmin,wmax,"lin");

  Set_CoherentRhoTerms(true);

  ex=SU_vector::Generator(nsun,1);
  ey=SU_vector::Generator(nsun,2);
  ez=SU_vector::Generator(nsun,3);

  B=ez;

  // set initial conditions for the density matrix.
  //integrate the fermi distribution to get the normalization
  double Norm=0;
  for(int ei = 0; ei < nx; ei++){
    double w=Get_x(ei);
    if(w>0) Norm+=(1.0/(w*w)*Fermi(1/(2*w)));
  }
  Norm=Norm*(w_max-w_min)/((double)nx);

  //set initial state
  for(int ei = 0; ei < nx; ei++){
    double w=Get_x(ei);
    if(w>0){
      state[ei].rho[0] = (ey*sin(theta) + ez*cos(theta))*(1.0/(Norm*w*w)*Fermi(1/(2*w)));
    }else{
      state[ei].rho[0] = (ey*sin(theta) + ez*cos(theta))*(-0.7/(Norm*w*w)*Fermi(-1/(2*w)));
    }
  }

  //setting errors and step function for the GSL
  Set_rel_error(1e-7);
  Set_abs_error(1e-7);
  Set_h(1e-10);
  Set_GSL_step(gsl_odeiv2_step_rk4);
  
  buf1.reset(new double[B.Size()]);
  buf2.reset(new double[B.Size()]);
}

void collective::PreDerive(double t){
  if(bar)
    progressbar(100*t/period, mu);
  
  //compute the sum of 'polarizations' of all nodes
  //note that here we use the temporary `estate`,
  //since this is during an evolution step, rather than the general `state`
  P=estate[0].rho[0];
  for(int ei = 1; ei < nx; ei++)
    P+=estate[ei].rho[0];
  //update the strength of self-interactions
  mu = mu_f+(mu_i-mu_f)*(1.0-t/period);
}

SU_vector collective::HI(unsigned int ix, unsigned int irho, double t) const{
  //the following is equivalent to
  //return Get_x(ix)*B+P*(mu*(w_max-w_min)/(double)nx);
  
  //make temporary vectors which use the preallocated buffers
  SU_vector t1(nsun,buf1.get());
  SU_vector t2(nsun,buf2.get());
  //evaluate the subexpressions into the temporaries
  t1=Get_x(ix)*B;
  t2=P*(mu*(w_max-w_min)/(double)nx);
  //return the sum of the temporaries, noting that t1 is no
  //longer needed so its storage may be overwritten
  return(std::move(t1)+t2);
}

void collective::Adiabatic_mu(double mui, double muf, double per, bool b){
  period=per;
  bar=b;
  mu_i=mui;
  mu_f=muf;
  Evolve(period);
  if(bar)
    std::cout << std::endl;  
}

double collective::Fermi(double EoverT){
  return 1.0/(exp(EoverT)+1.0);
}
