function S = UpdateSparseCodes(T, D, lambda, S_initial, alpha, beta)
%function S = UpdateSparseCodes(T, D, lambda, S_initial, alpha, beta)
%
% T - sufficient statistics for the data
% D - dictionary of size n_dimensions by n_atoms
% lambda - l1-norm regularization parameter
% S_initial - initial guess for sparse codes of all points, size is n_atoms by n_points
% alpha - sufficient decrease parameter for Armijo rule
% beta - 0 < beta < 1 - specifies geometric rate of decrease of line search parameter


if nargin == 4
  alpha = 1e-4;
  beta = 0.9;
elseif nargin == 5
  beta = 0.9;
end

obj_tol = 1e-6;

[d k] = size(D);

n = size(T, 2);

% just one sparse code for now

% Notes for future efficiency improvements:
%   It seems that the rank of AtA should not change when coding
% different points.
%   We should precompute DtD and then scale its rows and columns
%   via Lambda

for i = 1:n

  s_0 = zeros(k, 1);
  
  fprintf('\t\tStarting Objective value: %f\n', ...
	  ComputeSparseCodesObjective(D, s_0, T(:,i), lambda));

  
  max_main_iterations = 10;
  main_iteration = 0;
  main_converged = false;
  while (main_iteration <= max_main_iterations) && ~main_converged

    main_iteration = main_iteration + 1;
    fprintf('Main Iteration %d\n', main_iteration);
  
    
    Lambda = exp(D * s_0);
    z = (T(:,i) ./ Lambda) - ones(d, 1) + D * s_0;
    regressors = bsxfun(@times, Lambda, D);
    targets = sqrt(Lambda) .* z;
    
    AtA = regressors' * regressors;
    %regressors
    %targets
    
    %fprintf('starting goodness: %f\n', norm(regressors * s_0 - targets));
    
    s_1 = l1ls_featuresign(regressors, targets, lambda / 2, s_0, AtA, ...
			   rank(AtA));
    %s_0
    %s_1
    
    
    f_0 = ComputeSparseCodesObjective(D, s_0, T(:,i), lambda);
    
    
    
    % choose a subgradient at s_0
    subgrad = -D' * T(:,i);
    subgrad = subgrad + D' * exp(D * s_0);
    subgrad = subgrad + lambda * ((s_0 > 0) - (s_0 < 0)); % handle possibly non-differentiable component by using subgradient
    
    
    % Armijo'ish line search to select next s
    t = 1;
    iteration_num = 0;
    done = false;
    prev_best_f = f_0;
    while ~done
      iteration_num = iteration_num + 1;
      
      s_t = t * s_1 + (1 - t) * s_0;
      
      f_t = ComputeSparseCodesObjective(D, s_t, T(:,i), lambda);
      
      if f_t <= f_0 + alpha * subgrad' * (s_t - s_0)
	done = true;
	fprintf('\t\tCompleted %d line search iterations\n', iteration_num);
	fprintf('\t\tObjective value: %f\n', f_t);
	
	if f_t > prev_best_f
	  error('Objective increased! Aborting...');
	  return;
	end
	if prev_best_f - f_t < obj_tol
	  fprintf('Improvement to objective below tolerance. Finished.\n');
	  main_converged = true;
	end
	prev_best_f = f_t;
      end
      t = beta * t;
    end
    s_0 = s_t;
  end
  
  S(:,i) = s_0;
  
end
