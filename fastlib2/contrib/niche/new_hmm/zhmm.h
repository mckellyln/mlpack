#ifndef HMM_H
#define HMM_H


#include "fastlib/fastlib.h"
#include "distribution.h"
#include "gaussian.h"

#define MULTINOMIAL 0
#define GAUSSIAN 1
#define MOG 2


template <typename TDistribution>
class HMM {

 private:

  int n_states_;
  int n_dims_;

  Vector p_initial_;

  /* rows sum to 1: p_transition_.get(i,j) is P(s_j | s_i) */
  Matrix p_transition_; //

  TDistribution* state_distributions_;
  
    

 public:

  void Init(int n_states_in, int n_dims_in) {
    n_states_ = n_states_in;
    n_dims_ = n_dims_in;
    
    p_initial_.Init(n_states_);
    p_transition_.Init(n_states_, n_states_);

    state_distributions_ = 
      (TDistribution*) malloc(n_states_ * sizeof(TDistribution));
    for(int i = 0; i < n_states_; i++) {
      state_distributions_[i].Init(n_dims_);
    }
  }

  
  void RandomlyInitialize() {
    double uniform = ((double) 1) / ((double) n_states_);

    for(int i = 0; i < n_states_; i++) {
      p_initial_[i] = uniform;
      for(int j = 0; j < n_states_; j++) {
	p_transition_.set(j, i, uniform);
      }
   
      //state_distributions_[i].Init(n_dims_);
      state_distributions_[i].RandomlyInitialize();
    }
  }


  void BaumWelch(ArrayList <Matrix> sequences>) {
    for(int i = 0; i < n_sequences; i++) {
      Matrix &sequence = sequences[i];
      int sequence_length = sequence.n_cols();
      Matrix p_x_given_q;
      p_x_given_q.Init(n_states_, sequence_length);
      ComputePxGivenQ(sequence, &p_x_given_q);

      ForwardAlgorithm(p_x_given_q, &scaling_vars, &forward_vars);
      BackwardAlgorithm(p_x_given_q, scaling_vars, &backward_vars);

      // p_qq_t = Rabiner's xi
      ComputePqqt(forward_vars, backward_vars, p_x_given_q, &p_qq_t);
      // p_qt = Rabiner's gamma
      ComputePqt(forward_vars ,backward_vars, scaling_vars, p_qq_t, &p_qt);

      update_numerator_p_initial(p_qt, &new_p_initial_numerator);
      update_denominator_p_initial(p_qt, &new_p_initial_denominator);
      update_numerator_p_transition(p_qq_t, &new_p_transition_numerator);
      update_denominator_p_transition(p_qt, &new_p_transition_denominator);
      //update distribution numerator and denominator
    }
    la::Scale(1 / new_p_initial_denominator, &new_p_initial_numerator);
    la::Scale(1 / new_p_transition_denominator, &new_p_transition_numerator);
    // use distribution numerator and denominator
    
    
    
  }



  void ComputePxGivenQ(const Matrix &sequence,
			Matrix* p_p_x_given_q) {
    Matrix &p_x_given_q = *p_p_x_given_q;

    int sequence_length = sequence.n_cols();
    
    for(int t = 0; t < sequence_length; t++) {
      Vector x_t;
      data.MakeColumnVector(t, &x_t);
      
      for(int i = 0; i < n_states_; i++) {
	p_x_given_q.set(i, t, state_distributions_[i].pdf(x_t));
	// ensure that class member function exists for classes:
	//                                                       Multinomial
	//                                                       Gaussian
	//                                                       MoG
      }
    }
  }

  void ForwardAlgorithm(const Matrix &p_x_given_q,
			Vector* p_scaling_vars, 
			Matrix* p_forward_vars) {
    Vector& scaling_vars = *p_scaling_vars;
    Matrix& forward_vars = *p_forward_vars;

    int sequence_length = p_x_given_q.n_cols();

    scaling_vars.Init(sequence_length);
    forward_vars.Init(n_states_, sequence_length);

    Vector forward_0;
    forward_vars.MakeColumnVector(0, &forward_0);

    Vector p_x0_given_q;
    p_x_given_q.MakeColumnVector(0, &p_x0_given_q);

    HadamardMultiplyOverwrite(p_initial_, p_x0_given_q, &forward_0);
    ScaleForwardVar(&(c[0]), &forward_0);

    for(int t = 0; t < T - 1; t++) {
      Vector forward_t;
      forward_vars.MakeColumnVector(t, &forward_t);

      Vector forward_t_plus_1;
      forward_vars.MakeColumnVector(t, &forward_t_plus_1);

      Vector p_xt_plus_1_given_q;
      p_x_given_q.MakeColumnVector(t+1, p_xt_plus_1_given_q);

      la::MulOverwrite(forward_t, p_transition_, &forward_t_plus_1);
      HadamardMultiplyBy(p_xt_plus_1, &forward_t_plus_1);
      ScaleForwardVar(&(c[t+1]), &forward_t_plus_1);
    }
  }

  void BackwardAlgorithm(const Matrix &p_x_given_q,
			 const Vector &scaling_vars,
			 Matrix *p_backward_vars) {
    Matrix& backward_vars = *p_backward_vars;
    
    int sequence_length = p_x_given_q.n_cols();
    
    backward_vars.Init(n_states_, sequence_length);
    
    Vector backward_T_minus_1;
    backward_vars.MakeColumnVector(sequence_length - 1, &backward_T_minus_1);
    
    backward_T_minus_1.SetAll(c[sequence_length - 1]);

    for(int t = sequence_length - 2; t >= 0; t--) {
      Vector backward_t;
      backward_vars.MakeColumnVector(t, &backward_t);

      Vector backward_t_plus_1;
      backward_vars.MakeColumnVector(t + 1, &backward_t_plus_1);

      Vector p_xt_plus_1_given_q;
      p_x_given_q.MakeColumnVector(t + 1, &p_xt_plus_1_given_q);

      Vector result;
      HadamardMultiplyInit(p_xt_plus_1_given_q, backward_t_plus_1,
			   &result);
      la::MulInit(p_transition_, result, &backward_t);
      la::Scale(c[t], &backward_t);
    }
  }


  void ComputePqqt(const Matrix &forward_vars,
		   const Matrix &backward_vars,
		   const Matrix& p_x_given_q,
		   ArrayList<Matrix>* p_p_qq_t);
  ArrayList<Matrix> &p_qq_t = *p_p_qq_t;
  
  int sequence_length = forward_vars.n_rows();
  
  p_qq_t.Init(n_states_);
  for(int i = 0; i < n_states_; i++) {
    p_qq_t[i].Init(n_states_, sequence_length);
  }

  for(int i = 0; i < n_states_; i++) {
    for(int t = 0; t < sequence_length - 1; t++) {
      for(int j = 0; j < n_states_; j++) {
	p_qq_t[i].set(j, t,
		      forward_vars.get(i, t) // consider transposing, profile?
		      * p_transition_.get(i, j) //consider transposing, ""?
		      * p_x_given_q.get(j, t + 1)
		      * backward_vars.get(j, t + 1));
      }
    }
  }

  void ComputePqt(const Matrix &forward_vars,
		  const Matrix &backward_vars,
		  const Vector &scaling_vars,
		  const ArrayList<Matrix> &p_qq_t,
		  Matrix* p_p_qt) {
    Matrix &p_qt = *p_p_qt;

    int sequence_length = forward_vars.length();

    p_qt.Init(sequence_length, n_states_);

    for(int i = 0; i < n_states_; i++) {
      for(int t = 0; t < sequence_length - 1; t++) {
	double sum = 0;
	for(int j = 0; j < n_states; j++) {
	  sum += p_qq_t[i].get(j, t);
	}
	p_qt.set(t, i, sum);
      }
      
      int t = sequence_length - 1;
      p_qt.set(t, i, 
	       forward_vars.get(i, sequence_length - 1)
	       * backward_vars.get(i, sequence_length - 1)
	       / scaling_vars.get(sequence_length - 1));
    }
  }

  void HadamardMultiplyInit(const Vector &x,
			    const Vector &y,
			    Vector* z) {
    z -> Init(x.length());
    for(int i = 0; i < x.length(); i++) {
      (*z)[i] = x[i] * y[i];
    }
  }

  void HadamardMultiplyOverwrite(const Vector &x,
				 const Vector &y,
				 Vector* z) {
    for(int i = 0; i < x.length(); i++) {
      (*z)[i] = x[i] * y[i];
    }
  }
 
  void HadamardMultiplyBy(const Vector &x,
			  Vector* y) {
    for(int i = 0; i < x.length(); i++) {
      (*y)[i] *= x[i];
    }
  }
 
  void ScaleForwardVar(double* c, Vector *p_forward_var) {
    (*c) = ((double)1) / Sum(*p_forward_var);
    la::Scale(*c, p_forward_var);
  }
 
  double Sum(const Vector &x) {
    double sum = 0;
    for(int i = 0; i < x.length(); i++) {
      sum += x[i];
    }
    return sum;
  }




  void SetStateDistribution(int i,
			    const TDistribution &distribution) {
    state_distributions_[i].CopyValues(distribution);
  }


  // accessors

  int n_states() const {
    return n_states_;
  }

  int n_dims() const {
    return n_dims_;
  }
  
  Vector p_initial() const {
    return p_initial_;
  }

  Matrix p_transition() const {
    return p_transition_;
  }

  TDistribution* state_distributions() const {
    return state_distributions_;
  }

  void PrintDebug(const char *name = "", FILE *stream = stderr) const {
    fprintf(stream, "----- HMM %s ------\n", name);
    
    p_initial_.PrintDebug("initial probabilities", stream);
    p_transition_.PrintDebug("transition probabilities", stream);

    char string[100];    
    for(int i = 0 ;i < n_states_; i++) {
      sprintf(string, "state %d:\n", i+1);
      state_distributions_[i].PrintDebug(string);
      fprintf(stream, "\n");
    }
  }

  void SetPInitial(Vector p_initial_in) {
    p_initial_.CopyValues(p_initial_in);
  }

  void SetPTransition(Matrix p_transition_in) {
    p_transition_.CopyValues(p_transition_in);
  }
  

  ~HMM() {
    //printf("destroying HMM\n");
    Destruct();
  }

  void Destruct() {
    for(int i = 0; i < n_states_; i++) {
      state_distributions_[i].Destruct();
    }
    free(state_distributions_);
  }

};


#endif /* HMM */
