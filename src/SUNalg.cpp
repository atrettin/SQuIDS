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
 *      Christopher Weaver (University of Wisconsin Madison)                   * 
 *         chris.weaver@icecube.wisc.edu                                       *
 *      Jordi Salvado (University of Wisconsin Madison)                        *
 *         jsalvado@icecube.wisc.edu                                           *
 ******************************************************************************/

#include "SUNalg.h"

#include <ostream>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_linalg.h>
#include <gsl/gsl_blas.h>

#define KRONECKER(i,j)  ( (i)==(j) ? 1 : 0 )

namespace squids{

/*
-----------------------------------------------------------------------
SU_vector implementation
-----------------------------------------------------------------------
*/


/*
-----------------------------------------------------------------------
Constructors
-----------------------------------------------------------------------
*/


SU_vector::SU_vector():
dim(0),
size(0),
components(nullptr),
isinit(false),
isinit_d(false){}

SU_vector::SU_vector(const SU_vector& V):
dim(V.dim),
size(V.size),
components(new double[size]),
isinit(true),
isinit_d(false){
  std::copy(V.components,V.components+size,components);
}

SU_vector::SU_vector(SU_vector&& V):
dim(V.dim),
size(V.size),
components(V.components),
isinit(V.isinit),
isinit_d(V.isinit_d){
  if(V.isinit){
    V.dim=0;
    V.size=0;
    V.components=nullptr;
    V.isinit=false;
  }
}

SU_vector::SU_vector(unsigned int d,double* comp):
dim(d),
size(dim*dim),
components(comp),
isinit(false),
isinit_d(true)
{
  if(dim>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::SU_vector(unsigned int, double*): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
};

SU_vector::SU_vector(unsigned int d):
dim(d),
size(dim*dim),
components(new double[size]),
isinit(true),
isinit_d(false)
{
  if(dim>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::SU_vector(unsigned int): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  std::fill(components,components+size,0.0);
};

SU_vector::SU_vector(const std::vector<double>& comp):
dim(sqrt(comp.size())),
size(comp.size()),
components(new double[size]),
isinit(true),
isinit_d(false)
{
  if(dim*dim!=size)
    throw std::runtime_error("SU_vector::SU_vector(std::vector<double>): Vector size must be a square");
  if(dim>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::SU_vector(std::vector<double>): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  std::copy(comp.begin(),comp.end(),components);
};

SU_vector::SU_vector(const gsl_matrix_complex* m):
dim(m->size1),
size(dim*dim),
components(new double[size]),
isinit(true),
isinit_d(false)
{
  if(m->size1!=m->size2)
    throw std::runtime_error("SU_vector::SU_vector(gsl_matrix_complex*): Matrix must be square");
  if(dim>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::SU_vector(gsl_matrix_complex*): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  
  std::fill(components,components+size,0.0);
  
  double m_real[dim][dim]; double m_imag[dim][dim];
  for(unsigned int i=0; i<dim; i++){
    for(unsigned int j=0; j<dim; j++){
      m_real[i][j] = gsl_matrix_complex_get(m,i,j).dat[0];
      m_imag[i][j] = gsl_matrix_complex_get(m,i,j).dat[1];
    }
  }
  // the following rules are valid ONLY when the initial
  // matrix is hermitian
  
#include "MatrixToSUSelect.txt"
};

namespace{
  struct sq_array_2D{
    unsigned int d;
    double* data;
    
    struct index_proxy{
      double* data;
      double operator[](unsigned int j) const{
        return(data[j]);
      }
    };
    index_proxy operator[](unsigned int i) const{
      return(index_proxy{data+d*i});
    }
  };
  
void ComponentsFromMatrices(double* components, unsigned int dim, const sq_array_2D& m_real, const sq_array_2D& m_imag){
#include "MatrixToSUSelect.txt"
}
}

SU_vector SU_vector::Projector(unsigned int d, unsigned int ii){
  if(d>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::Projector(unsigned int, unsigned int): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  if(ii>=d)
    throw std::runtime_error("SU_vector::Projector(unsigned int, unsigned int): Invalid component: must be smaller than dimension");
  
  double m_real[d][d]; double m_imag[d][d];
  for(unsigned int i=0; i<d; i++){
    for(unsigned int j=0; j<d; j++){
      m_real[i][j] = KRONECKER(i,j)*KRONECKER(i,ii);
      m_imag[i][j] = 0.0;
    }
  }
  
  SU_vector v(d);
  ComponentsFromMatrices(v.components,d,sq_array_2D{d,m_real[0]},sq_array_2D{d,m_imag[0]});
  return(v);
}

SU_vector SU_vector::Identity(unsigned int d){
  if(d>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::Identity(unsigned int): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  
  double m_real[d][d]; double m_imag[d][d];
  for(unsigned int i=0; i<d; i++){
    for(unsigned int j=0; j<d; j++){
      m_real[i][j] = KRONECKER(i,j);
      m_imag[i][j] = 0.0;
    }
  }
  
  SU_vector v(d);
  ComponentsFromMatrices(v.components,d,sq_array_2D{d,m_real[0]},sq_array_2D{d,m_imag[0]});
  return(v);
}

SU_vector SU_vector::PosProjector(unsigned int d, unsigned int ii){
  if(d>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::PosProjector(unsigned int, unsigned int): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  if(ii>=d)
    throw std::runtime_error("SU_vector::PosProjector(unsigned int, unsigned int): Invalid component: must be smaller than dimension");
  
  double m_real[d][d]; double m_imag[d][d];
  for(unsigned int i=0; i<d; i++){
    for(unsigned int j=0; j<d; j++){
      if(i<ii)
        m_real[i][j] = KRONECKER(i,j);
      else
        m_real[i][j] = 0.0;
      m_imag[i][j] = 0.0;
    }
  }
  
  SU_vector v(d);
  ComponentsFromMatrices(v.components,d,sq_array_2D{d,m_real[0]},sq_array_2D{d,m_imag[0]});
  return(v);
}

SU_vector SU_vector::NegProjector(unsigned int d, unsigned int ii){
  if(d>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::NegProjector(unsigned int, unsigned int): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  if(ii>=d)
    throw std::runtime_error("SU_vector::NegProjector(unsigned int, unsigned int): Invalid component: must be smaller than dimension");
  
  double m_real[d][d]; double m_imag[d][d];
  for(unsigned int i=0; i<d; i++){
    for(unsigned int j=0; j<d; j++){
      if(d-i<ii)
        m_real[i][j] = KRONECKER(i,j);
      else
        m_real[i][j] = 0.0;
      m_imag[i][j] = 0.0;
    }
  }
  
  SU_vector v(d);
  ComponentsFromMatrices(v.components,d,sq_array_2D{d,m_real[0]},sq_array_2D{d,m_imag[0]});
  return(v);
}

SU_vector SU_vector::Generator(unsigned int d, unsigned int ii){
  if(d>SQUIDS_MAX_HILBERT_DIM)
    throw std::runtime_error("SU_vector::Component(unsigned int, unsigned int): Invalid size: only up to SU(" SQUIDS_MAX_HILBERT_DIM_STR ") is supported");
  if(ii>=d*d)
    throw std::runtime_error("SU_vector::Component(unsigned int, unsigned int): Invalid component: must be smaller than dimension");
  SU_vector v(d);
  v.components[ii] = 1.0;
  return(v);
}


/*
-----------------------------------------------------------------------
Operations
-----------------------------------------------------------------------
*/

void SU_vector::SetAllComponents(double x){  
  for(unsigned int i = 0; i < size; i++)
    components[i] = x;
}

std::vector<double> SU_vector::GetComponents() const{
  std::vector<double> x ( dim*dim );
  for (unsigned int i = 0; i < dim*dim ; i++)
    x[i] = components[i];
  return x;
}

std::unique_ptr<gsl_matrix_complex,void (*)(gsl_matrix_complex*)>
SU_vector::GetGSLMatrix() const {
  if( !isinit and !isinit_d)
    throw std::runtime_error("SU_vector::GetGSLMatrix(): SU_vector not initialized.");
  gsl_matrix_complex * matrix = gsl_matrix_complex_alloc(dim,dim);
#include "SUToMatrixSelect.txt"
  return std::unique_ptr<gsl_matrix_complex,void (*)(gsl_matrix_complex*)>(matrix,gsl_matrix_complex_free);
}


  void gsl_complex_matrix_exponential(gsl_matrix_complex *eA, gsl_matrix_complex *A, int dimx)
  {
    int j,k=0;
    gsl_complex temp;
    gsl_matrix *matreal =gsl_matrix_alloc(2*dimx,2*dimx);
    gsl_matrix *expmatreal =gsl_matrix_alloc(2*dimx,2*dimx);
    //Converting the complex matrix into real one using A=[Areal, Aimag;-Aimag,Areal]
    for (j = 0; j < dimx;j++)
      for (k = 0; k < dimx;k++)
        {
	  temp=gsl_matrix_complex_get(A,j,k);
	  gsl_matrix_set(matreal,j,k,GSL_REAL(temp));
	  gsl_matrix_set(matreal,dimx+j,dimx+k,GSL_REAL(temp));
	  gsl_matrix_set(matreal,j,dimx+k,GSL_IMAG(temp));
	  gsl_matrix_set(matreal,dimx+j,k,-GSL_IMAG(temp));
        }

    gsl_linalg_exponential_ss(matreal,expmatreal,GSL_PREC_DOUBLE);

    double realp;
    double imagp;
    for (j = 0; j < dimx;j++)
      for (k = 0; k < dimx;k++)
        {
	  realp=gsl_matrix_get(expmatreal,j,k);
	  imagp=gsl_matrix_get(expmatreal,j,dimx+k);
	  gsl_matrix_complex_set(eA,j,k,gsl_complex_rect(realp,imagp));
        }
    gsl_matrix_free(matreal);
    gsl_matrix_free(expmatreal);
  }

  void gsl_matrix_complex_change_basis_UCMU(gsl_matrix_complex* U, gsl_matrix_complex* M){
    int numneu = U->size1;
    gsl_matrix_complex *U1 = gsl_matrix_complex_alloc(numneu,numneu);
    gsl_matrix_complex *U2 = gsl_matrix_complex_alloc(numneu,numneu);
    gsl_matrix_complex_memcpy(U1,U);
    gsl_matrix_complex_memcpy(U2,U);
 
    gsl_matrix_complex *T1 = gsl_matrix_complex_alloc(numneu,numneu);
    
    // doing : U M U^dagger
    
    gsl_blas_zgemm(CblasNoTrans,CblasNoTrans,
                   gsl_complex_rect(1.0,0.0),M,
                   U1,gsl_complex_rect(0.0,0.0),T1);
    gsl_blas_zgemm(CblasConjTrans,CblasNoTrans,
                   gsl_complex_rect(1.0,0.0),U2,
                   T1,gsl_complex_rect(0.0,0.0),M);
    // now H_current is in the interaction basis of the mass basis
    
    gsl_matrix_complex_free(U1);
    gsl_matrix_complex_free(U2);
    gsl_matrix_complex_free(T1);
  }
  
  
  SU_vector SU_vector::UTransform(const SU_vector& v){
    auto mv=v.GetGSLMatrix().get();
    auto mu=(*this).GetGSLMatrix().get();
    gsl_matrix_complex * outmat = gsl_matrix_complex_alloc (size, size);
    gsl_matrix_complex * em = gsl_matrix_complex_alloc (size, size);
    
    gsl_complex_matrix_exponential(em,mv,size);    
    gsl_matrix_complex_change_basis_UCMU(em, mv);
      
    SU_vector out((const gsl_matrix_complex*) mv);
    return out;
  }

SU_vector SU_vector::Rotate(unsigned int ii, unsigned int jj, double th, double del) const{
  const SU_vector& suv=*this;
  SU_vector suv_rot(dim);
  unsigned int i=ii+1, j=jj+1; //convert to 1 based indices to interface with Mathematica generated code

  assert(i<j && "Components selected for rotation must be in ascending order");
  
#include "rotation_switcher.h"
    
  return suv_rot;
}

void SU_vector::RotateToB0(const Const& param){
  for(unsigned int j=dim-1; j>0; j--){
    for(unsigned int i=j-1; i<dim; i--)
      *this=Rotate(i,j,-param.GetMixingAngle(i,j),-param.GetPhase(i,j)); //note negated angle, and phase
  }
}

void SU_vector::RotateToB1(const Const& param){
  for(unsigned int j=dim-1; j>0; j--){
    for(unsigned int i=j-1; i<dim; i--)
      *this=Rotate(i,j,param.GetMixingAngle(i,j),param.GetPhase(i,j));
  }
}

double SUTrace(const SU_vector& suv1,const SU_vector& suv2){
  double gen_trace = 0.0;
  double id_trace = 0.0;

  for(unsigned int i=1; i < suv1.Size(); i++)
    gen_trace += (suv1.components[i])*(suv2.components[i]);

  id_trace = (suv1.components[0])*(suv2.components[0])*double(suv1.dim);
  return id_trace+2.0*gen_trace;
}

bool SU_vector::operator==(const SU_vector& other) const{
  if(dim != other.dim) //vectors of different sizes cannot be equal
    return false;
  //if one vector contains data and the other does not they cannot be equal
  if((isinit || isinit_d) != (other.isinit || other.isinit_d))
    return false;
  //if both vectors are empty, consider them equal
  if(!isinit && !isinit_d)
    return true;
  //test whether all components are equal
  for(unsigned int i=0; i < size; i++){
    if(components[i] != other.components[i])
      return false;
  }
  return true;
}

double SU_vector::operator*(const SU_vector &other) const{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector inner product");
  return SUTrace(*this,other);
}

detail::MultiplicationProxy SU_vector::operator*(double x) const &{
  return(detail::MultiplicationProxy{*this,x});
}
detail::MultiplicationProxy SU_vector::operator*(double x) &&{
  return(detail::MultiplicationProxy{*this,x,detail::Arg1Movable});
}

detail::MultiplicationProxy operator*(double x, const SU_vector& v){
  return(detail::MultiplicationProxy{v,x});
}
detail::MultiplicationProxy operator*(double x, SU_vector&& v){
  return(detail::MultiplicationProxy{v,x,detail::Arg1Movable});
}

SU_vector& SU_vector::operator=(const SU_vector& other){
  if(this==&other)
    return(*this);
  if(size!=other.size){
    if(isinit_d) //can't resize
      throw std::runtime_error("Non-matching dimensions in assignment to SU_vector with external storage");
    //can resize
    if(isinit)
      delete[] components;
    dim=other.dim;
    size=other.size;
    components=new double[size];
    isinit=true;
  }
  
  std::copy(other.components,other.components+size,components);
  return *this;
}

SU_vector& SU_vector::operator=(SU_vector&& other){
  if(this==&other)
    return(*this);
  if(isinit){ //this vector owns storage
    std::swap(dim,other.dim);
    std::swap(size,other.size);
    std::swap(components,other.components);
    std::swap(isinit,other.isinit);
    std::swap(isinit_d,other.isinit_d);
  }
  else if(isinit_d){ //this vector has non-owned storage
    //conservatively assume that we cannot shift our storage, so fall back on an actual copy
    if(size!=other.size)
      throw std::runtime_error("Non-matching dimensions in assignment to SU_vector with external storage");
    std::copy(other.components,other.components+size,components);
  }
  else{
    //this vector has no storage, so just take everything from other
    dim = other.dim;
    size = other.size;
    components = other.components;
    isinit = other.isinit;
    isinit_d = other.isinit_d;
    if(other.isinit){ //other must relinquish ownership
      other.dim=0;
      other.size=0;
      other.components=nullptr;
      other.isinit=false;
    }
  }
  return(*this);
}

SU_vector& SU_vector::operator+=(const SU_vector &other){
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector increment");
  for(unsigned int i=0; i < size; i++)
    components[i] += other.components[i];
  return *this;
}

SU_vector& SU_vector::operator-=(const SU_vector &other){
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector decrement");
  for(unsigned int i=0; i < size; i++)
    components[i] -= other.components[i];
  return *this;
}

SU_vector& SU_vector::operator *=(double x){
  for(unsigned int i = 0; i < size; i++)
    components[i] *= x;
  return *this;
}

SU_vector& SU_vector::operator /=(double x){
  for(unsigned int i = 0; i < size; i++)
    components[i] /= x;
  return *this;
}

detail::AdditionProxy SU_vector::operator+(const SU_vector& other) const &{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector addition");
  return(detail::AdditionProxy{*this,other});
}

detail::AdditionProxy SU_vector::operator+(SU_vector&& other) const &{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector addition");
  //exploit commutativity and put the movable object first
  return(detail::AdditionProxy{other,*this,detail::Arg1Movable});
}

detail::AdditionProxy SU_vector::operator+(const SU_vector& other) &&{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector addition");
  return(detail::AdditionProxy{*this,other,detail::Arg1Movable});
}

detail::AdditionProxy SU_vector::operator+(SU_vector&& other) &&{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector addition");
  return(detail::AdditionProxy{*this,other,detail::Arg1Movable|detail::Arg2Movable});
}

detail::SubtractionProxy SU_vector::operator-(const SU_vector& other) const &{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector subtraction");
  return(detail::SubtractionProxy{*this,other});
}

detail::SubtractionProxy SU_vector::operator-(const SU_vector& other) &&{
  if(size!=other.size)
    throw std::runtime_error("Non-matching dimensions in SU_vector subtraction");
  return(detail::SubtractionProxy{*this,other,detail::Arg1Movable});
}

detail::NegationProxy SU_vector::operator-() const &{
  return(detail::NegationProxy{*this});
}

detail::NegationProxy SU_vector::operator-() &&{
  return(detail::NegationProxy{*this,detail::Arg1Movable});
}

detail::EvolutionProxy SU_vector::Evolve(const SU_vector& suv1,double t) const{
  return(detail::EvolutionProxy{suv1,*this,t});
}

std::ostream& operator<<(std::ostream& os, const SU_vector& V){
  for(unsigned int i=0; i< V.size-1; i++)
    os << V.components[i] << "  ";
  os << V.components[V.size-1];
  return os;
}

detail::iCommutatorProxy iCommutator(const SU_vector& suv1,const SU_vector& suv2){
  if(suv1.dim!=suv2.dim)
    throw std::runtime_error("Commutator error, non-matching dimensions ");
  return(detail::iCommutatorProxy{suv1,suv2});
}
detail::ACommutatorProxy ACommutator(const SU_vector& suv1,const SU_vector& suv2){
  if(suv1.dim!=suv2.dim)
    throw std::runtime_error("Anti Commutator error: non-matching dimensions ");
  return(detail::ACommutatorProxy{suv1,suv2});
}

} //namespace squids
