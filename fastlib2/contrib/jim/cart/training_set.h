#ifndef TRAINING_SET_H
#define TRAINING_SET_H

#include "fastlib/fastlib.h"


class TrainingSet{ 

 private:
  Dataset data_;
  Matrix* data_matrix_;
  ArrayList<Vector> order_;  
  ArrayList<Vector> back_order_; 
  //  ArrayList<int> old_from_new_;
  int n_features_, n_points_;


  int SortOrdinalFeature_(int dim, int start_, int stop_){   
    if (stop_ - start_ == 1){
      if (!isnan(data_matrix_->get(dim, start_))){
	order_[dim][start_] = -1;
	return start_;
      } else {
	order_[dim][start_] = -2;
	return -2;	
      }      
    } 
    int halfway = (start_ + stop_) / 2;      
    int left_start = SortOrdinalFeature_(dim, start_, halfway);
    int right_start = SortOrdinalFeature_(dim, halfway, stop_);     
    // Merge Results
    int merge_start, left, right;
    if (left_start >= 0){
      if (right_start >= 0 ) {
	if (data_matrix_->get(dim, left_start) < 
	    data_matrix_->get(dim, right_start)){
	  merge_start = left_start;
	  left = (int)order_[dim][left_start];
	  right = right_start;
	} else {
	  merge_start = right_start;
	  right = (int)order_[dim][right_start];
	  left = left_start; 
	}
	int current = merge_start;
	while(left >= 0 & right >= 0){
	  if (data_matrix_->get(dim, right) < data_matrix_->get(dim, left)){
	    order_[dim][current] = right;
	    current = right;
	    right = (int)order_[dim][right]; 
	  } else {
	    order_[dim][current] = left;
	    current = left;
	    left = (int)order_[dim][left];
	  }
	}
	if (left >= 0) {
	  order_[dim][current] = left;
	} else {
	  order_[dim][current] = right; 
	}
	return merge_start;
      } else {
	return left_start;
      }
    } else {
      if (right_start >= 0){
	return right_start;
      } else {
	return -1;
      }
    }
  }


  //////////////////////// Constructors ///////////////////////////////////////

  FORBID_ACCIDENTAL_COPIES(TrainingSet);

 public: 

  TrainingSet(){    
  }

  ~TrainingSet(){
  }

  ////////////////////// Helper Functions /////////////////////////////////////

  void Init(const char* fp, Vector &firsts){
   
    data_.InitFromFile(fp);
    // Make linked list representing sorting of ordered vars
    data_matrix_ = &data_.matrix();
    int n_features_ = data_.n_features();
    int n_points_ = data_.n_points();   
    order_.Init(n_features_);
    back_order_.Init(n_features_);     
    firsts.Init(n_features_);
    int i;
    DatasetInfo meta_data = data_.info();
    const DatasetFeature* current_feature;
    for (i = 0; i < n_features_; i++){
      current_feature = &meta_data.feature(i);      
      if (current_feature->type() != 2 ){
	order_[i].Init(n_points_); 
	back_order_[i].Init(n_points_);
	back_order_[i].SetAll(-2);
	firsts[i] = SortOrdinalFeature_(i, 0, n_points_);
	int j_old = (int)firsts[i], j_cur = (int)order_[i][(int)firsts[i]];
	back_order_[i][j_old] = -1;
	while(j_cur > 0){
	  back_order_[i][j_cur] = j_old;
	  j_old = j_cur;
	  j_cur = (int)order_[i][j_cur];
	} 	
      } else {
	order_[i].Init(0);
	back_order_[i].Init(0);
      }
    }    
  }

  // This function swaps columns of our data matrix, to represent the
  // partition into left and right nodes.
  index_t MatrixPartition(index_t start, index_t stop, index_t *old_from_new,
                          Vector split, Vector firsts, Vector* firsts_l_out, 
			  Vector* firsts_r_out){
    Vector firsts_l;
    Vector firsts_r;
    n_features_ = data_.n_features();
    firsts_l.Init(n_features_);
    firsts_r.Init(n_features_);

    int i;          
    int current_index;
    for (i = 0; i < n_features_; i++) {
      int right_index = -1, left_index = -1;
      if (order_[i].length() > 0) {
	current_index = (int)firsts[i];
	while (current_index >= 0) {
	  if (split[current_index - start] == 1){
	    if (left_index < 0){
	      firsts_l[i] = current_index;	       
	    } else {
	       order_[i][left_index] = current_index;
	    }
	    back_order_[i][current_index] = left_index;
	    left_index = current_index;	    
	  } else {
	    if (right_index < 0) {
	      firsts_r[i] = current_index;	      
	    } else {
	      order_[i][right_index] = current_index;
	    }
	    back_order_[i][current_index] = right_index;	   
	    right_index = current_index;	   
	   }
	  current_index = (int)order_[i][current_index];
	}
	if (left_index >= 0){
	  order_[i][left_index] = -1;
	}
	if (right_index >= 0){
	  order_[i][right_index] = -1;	 
	} 
	
      }
    }
   
    index_t left = start;
    index_t right = stop-1;
    for(;;){
      while (split[left - start] > 0 && likely(left <= right)){
        left++;
      }
      while (likely(left <= right) && split[right - start] < 1){
        right--;
      }
      
      if (unlikely(left > right)){
	break;
      }
   
      Vector left_vector;
      Vector right_vector;
      
      data_matrix_->MakeColumnVector(left, &left_vector);
      data_matrix_->MakeColumnVector(right, &right_vector);
      
      left_vector.SwapValues(&right_vector);
      int temp = (int)split[left - start];
      split[left - start] = split[right - start];
      split[right - start] = temp;
            
      // Update linked lists
      for (i = 0; i < n_features_; i++){
	if (order_[i].length() > 0){
	  int order_l, order_r, back_l, back_r;
	  order_l = (int)order_[i][left];
	  order_r = (int)order_[i][right];
	  back_l = (int)back_order_[i][left];
	  back_r = (int)back_order_[i][right];
	 
	  if (likely(order_l >= 0)){
	    back_order_[i][order_l] = right;	    
	  } 
	  order_[i][left] = order_r;
	  
	  if (likely(order_r >= 0)){
	    back_order_[i][order_r] = left;
	  }
	  order_[i][right] = order_l;

	  if (likely(back_l >= 0)){
	    order_[i][back_l] = right;	  
	  } else if (back_l == -1){
	    firsts_r[i] = right;
	  } 
	  back_order_[i][right] = back_l;
	  
	  if (likely(back_r >= 0)){
	    order_[i][back_r] = left;
	  } else if (back_r == -1){
	    firsts_l[i] = left;
	  }	 
	  back_order_[i][left] = back_r;	  
	}
      }      
      
      if (old_from_new){
	index_t t = old_from_new[left];
	old_from_new[left] = old_from_new[right];
	old_from_new[right] = t;	  
      }
      
      DEBUG_ASSERT(left <= right);
      right--;      
    }
    
    DEBUG_ASSERT(left == right + 1);
    firsts_l_out->Init(n_features_);
    firsts_r_out->Init(n_features_);
    firsts_l_out->CopyValues(firsts_l);
    firsts_r_out->CopyValues(firsts_r);
    
    return left;
  } //MatrixPartition
  


  int GetVariableType(int dim) {
    DatasetFeature temp = data_.info().feature(dim);
    return temp.n_values();
  }

  int GetFeatures(){
    return data_.n_features();   
  }

  int GetPointSize(){
    return data_.n_points();
  }

  int GetTargetType(int target_dim){
    DatasetFeature temp = data_.info().feature(target_dim);
    return temp.n_values();
  }
 

  void GetOrder(int dim, Vector *order, int start, int stop){
    order->WeakCopy(order_[dim]);    
  }

  double Get(int i, int j){
    return data_.get(i,j);
  }

}; // class DataSet

#endif
