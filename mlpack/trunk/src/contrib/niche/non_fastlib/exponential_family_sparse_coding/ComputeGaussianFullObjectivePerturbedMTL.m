function f = ComputeGaussianFullObjectivePerturbedMTL(D, Q, S, X, ...
						  lambda_S, lambda_Q, ...
						  point_inds)
%function f = ComputeGaussianFullObjectivePerturbedMTL(D, Q, S, X, ...
%						  lambda_S, lambda_Q, ...
%						  point_inds)
%
% Compute objective

n_tasks = length(Q);

f = 0;
for t = 1:n_tasks
  E = D + Q{t};
  inds = point_inds{t};

  f = f ...
      + -trace(E' * X(:,inds) * S(:,inds)') ...
      + 0.5 * norm(E * S(:,inds), 'fro')^2 ...
      + lambda_Q * sum(sum(abs(Q{t})));
end

f = f + lambda_S * sum(sum(abs(S)));
