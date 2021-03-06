/*
 * File:        COMPUTE_REINIT_EQN_RHS_2D.c
 * Copyrights:  (c) 2005 The Trustees of Princeton University and Board of
 *                  Regents of the University of Texas.  All rights reserved.
 *              (c) 2009 Kevin T. Chu.  All rights reserved.
 * Revision:    $Revision$
 * Modified:    $Date$
 * Description: MATLAB MEX-file for adding the contribution from a
 *              normal velocity term to the RHS of level set equation
 *            
 */

/*=======================================================================
 *
 * COMPUTE_REINIT_EQN_RHS_2D() computes the right-hand side of the
 * reinitialization equation.
 *
 * Usage:  reinit_rhs = COMPUTE_REINIT_EQN_RHS_2D( ...
 *                        phi, ghostcell_width, ...
 *                        phi_x_plus, phi_y_plus, ...
 *                        phi_x_minus, phi_y_minus, ...
 *                        dX)
 *
 * Arguments:
 * - phi:               level set function
 * - ghostcell_width:   ghostcell width for phi
 * - phi_x_plus:        x-component of plus HJ ENO derivative
 * - phi_y_plus:        y-component of plus HJ ENO derivative
 * - phi_x_minus:       x-component of minus HJ ENO derivative
 * - phi_y_minus:       y-component of minus HJ ENO derivative
 * - dX:                array containing the grid spacing
 *                        in coordinate directions
 *
 * Return value:
 * - reinit_rhs:        right-hand side of reinitialization equation
 *
 * NOTES:
 * - The phi_x_plus, phi_y_plus, phi_x_minus, and phi_y_minus arrays 
 *   are assumed to be the same size
 *
 * - All data arrays are assumed to be in the order generated by the 
 *   MATLAB meshgrid() function.  That is, data corresponding to the 
 *   point (x_i,y_j) is stored at index (j,i).
 *
 * - The returned reinit_rhs array is the same size as phi.  However, only
 *   the values of the RHS of the reinitialization evolution equation 
 *   within the _interior_ of the computational grid are computed.  In 
 *   other words, values of the RHS in the ghostcells are _not_ computed; 
 *   the value in the ghostcells is set to 0.
 *
 *=======================================================================*/

#include "mex.h"
#include "matrix.h"
#include "LSMLIB_config.h"
#include "lsm_reinitialization2d.h"

/* Input Arguments */ 
#define PHI             (prhs[0])
#define GHOSTCELL_WIDTH (prhs[1])
#define PHI_X_PLUS      (prhs[2])
#define PHI_Y_PLUS      (prhs[3])
#define PHI_X_MINUS     (prhs[4])
#define PHI_Y_MINUS     (prhs[5])
#define DX              (prhs[6])

/* Output Arguments */ 
#define REINIT_RHS      (plhs[0])

/* Macros */ 
#define NDIM            (2)


void mexFunction( int nlhs, mxArray *plhs[], 
		  int nrhs, const mxArray*prhs[] )
     
{ 

  /* data variables */
  LSMLIB_REAL *reinit_rhs; 
  int ilo_reinit_rhs_gb, ihi_reinit_rhs_gb;
  int jlo_reinit_rhs_gb, jhi_reinit_rhs_gb;
  LSMLIB_REAL *phi; 
  int ilo_phi_gb, ihi_phi_gb, jlo_phi_gb, jhi_phi_gb;
  int phi_ghostcell_width;
  LSMLIB_REAL *phi_x_plus, *phi_y_plus;
  LSMLIB_REAL *phi_x_minus, *phi_y_minus;
  int ilo_grad_phi_gb, ihi_grad_phi_gb;
  int jlo_grad_phi_gb, jhi_grad_phi_gb;
  int ilo_fb, ihi_fb, jlo_fb, jhi_fb;
  double *dX;
  LSMLIB_REAL dX_meshgrid_order[2];

  /* array dimension information */
  int num_data_array_dims;

  /* auxilliary variables */
  int num_grid_cells;
  int i;  
  int use_phi0_for_sgn = 0;
  
  /* Check for proper number of arguments */
  
  if (nrhs != 7) { 
    mexErrMsgTxt("Seven input arguments required."); 
  } else if (nlhs > 1) {
    mexErrMsgTxt("Too many output arguments."); 
  } 

  /* Check that the inputs have the correct floating-point precision */
#ifdef LSMLIB_DOUBLE_PRECISION
    if (!mxIsDouble(PHI)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for double-precision but phi is single-precision");
    }
    if (!mxIsDouble(PHI_X_PLUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for double-precision but phi_x_plus is single-precision");
    }
    if (!mxIsDouble(PHI_Y_PLUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for double-precision but phi_y_plus is single-precision");
    }
    if (!mxIsDouble(PHI_X_MINUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for double-precision but phi_x_minus is single-precision");
    }
    if (!mxIsDouble(PHI_Y_MINUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for double-precision but phi_y_minus is single-precision");
    }
#else      
    if (!mxIsSingle(PHI)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for single-precision but phi is double-precision");
    }
    if (!mxIsSingle(PHI_X_PLUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for single-precision but phi_x_plus is double-precision");
    }
    if (!mxIsSingle(PHI_Y_PLUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for single-precision but phi_y_plus is double-precision");
    }
    if (!mxIsSingle(PHI_X_MINUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for single-precision but phi_x_minus is double-precision");
    }
    if (!mxIsSingle(PHI_Y_MINUS)) {
      mexErrMsgTxt("Incompatible precision: LSMLIB built for single-precision but phi_y_minus is double-precision");
    }
#endif
    
  /* Parameter Checks */
  num_data_array_dims = mxGetNumberOfDimensions(PHI);
  if (num_data_array_dims != 2) {
    mexErrMsgTxt("phi should be a 2 dimensional array."); 
  }

  /* Assign pointers for phi and grad(phi) */
  phi         = (LSMLIB_REAL*) mxGetPr(PHI);
  phi_x_plus  = (LSMLIB_REAL*) mxGetPr(PHI_X_PLUS);
  phi_y_plus  = (LSMLIB_REAL*) mxGetPr(PHI_Y_PLUS);
  phi_x_minus = (LSMLIB_REAL*) mxGetPr(PHI_X_MINUS);
  phi_y_minus = (LSMLIB_REAL*) mxGetPr(PHI_Y_MINUS);

      
  /* Get size of data */
  ilo_phi_gb = 1;
  ihi_phi_gb = mxGetM(PHI);
  jlo_phi_gb = 1;
  jhi_phi_gb = mxGetN(PHI);

  ilo_grad_phi_gb = 1;
  ihi_grad_phi_gb = mxGetM(PHI_X_PLUS);
  jlo_grad_phi_gb = 1;
  jhi_grad_phi_gb = mxGetN(PHI_X_PLUS);

  /* if necessary, shift ghostbox for grad(phi) to be */ 
  /* centered with respect to the ghostbox for phi.  */
  if (ihi_grad_phi_gb != ihi_phi_gb) {
    int shift = (ihi_phi_gb-ihi_grad_phi_gb)/2;
    ilo_grad_phi_gb += shift;
    ihi_grad_phi_gb += shift;
  }
  if (jhi_grad_phi_gb != jhi_phi_gb) {
    int shift = (jhi_phi_gb-jhi_grad_phi_gb)/2;
    jlo_grad_phi_gb += shift;
    jhi_grad_phi_gb += shift;
  }

  /* Create matrix for RHS of reinitialization equation */
  ilo_reinit_rhs_gb = ilo_phi_gb;
  ihi_reinit_rhs_gb = ihi_phi_gb;
  jlo_reinit_rhs_gb = jlo_phi_gb;
  jhi_reinit_rhs_gb = jhi_phi_gb;
#ifdef LSMLIB_DOUBLE_PRECISION
  REINIT_RHS = mxCreateDoubleMatrix(ihi_reinit_rhs_gb-ilo_reinit_rhs_gb+1,
                                    jhi_reinit_rhs_gb-jlo_reinit_rhs_gb+1,
                                    mxREAL);
#else
  REINIT_RHS = mxCreateNumericMatrix(ihi_reinit_rhs_gb-ilo_reinit_rhs_gb+1,
                                     jhi_reinit_rhs_gb-jlo_reinit_rhs_gb+1,
                                     mxSINGLE_CLASS, mxREAL);
#endif

  reinit_rhs = (LSMLIB_REAL*) mxGetPr(REINIT_RHS); 

  /* zero out REINIT_RHS */
  num_grid_cells = (ihi_reinit_rhs_gb-ilo_reinit_rhs_gb+1)
                 * (jhi_reinit_rhs_gb-jlo_reinit_rhs_gb+1);
  for (i = 0; i < num_grid_cells; i++, reinit_rhs++) {
    (*reinit_rhs) = 0.0;
  }

  /* reset reinit_rhs to start of data array */
  reinit_rhs = (LSMLIB_REAL*) mxGetPr(REINIT_RHS);  

  /* Get dX */
  dX = mxGetPr(DX);

  /* Change order of dX to be match MATLAB meshgrid() order for grids. */
  dX_meshgrid_order[0] = (LSMLIB_REAL) dX[1];
  dX_meshgrid_order[1] = (LSMLIB_REAL) dX[0];

  /* Do the actual computations in a Fortran 77 subroutine */
  phi_ghostcell_width = mxGetPr(GHOSTCELL_WIDTH)[0];
  ilo_fb = ilo_reinit_rhs_gb + phi_ghostcell_width;
  ihi_fb = ihi_reinit_rhs_gb - phi_ghostcell_width;
  jlo_fb = jlo_reinit_rhs_gb + phi_ghostcell_width;
  jhi_fb = jhi_reinit_rhs_gb - phi_ghostcell_width;
  LSM2D_COMPUTE_REINITIALIZATION_EQN_RHS(
    reinit_rhs,
    &ilo_reinit_rhs_gb, &ihi_reinit_rhs_gb,
    &jlo_reinit_rhs_gb, &jhi_reinit_rhs_gb,
    phi,
    &ilo_phi_gb, &ihi_phi_gb,
    &jlo_phi_gb, &jhi_phi_gb,
    phi,
    &ilo_phi_gb, &ihi_phi_gb,
    &jlo_phi_gb, &jhi_phi_gb,
    phi_x_plus, phi_y_plus, 
    &ilo_grad_phi_gb, &ihi_grad_phi_gb,
    &jlo_grad_phi_gb, &jhi_grad_phi_gb,
    phi_x_minus, phi_y_minus, 
    &ilo_grad_phi_gb, &ihi_grad_phi_gb,
    &jlo_grad_phi_gb, &jhi_grad_phi_gb,
    &ilo_fb, &ihi_fb, 
    &jlo_fb, &jhi_fb,
    &dX_meshgrid_order[0], &dX_meshgrid_order[1], 
    &use_phi0_for_sgn);

  return;
}
