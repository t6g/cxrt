
static char help[] = "Solves one dimensional advection diffusion equation    \n\
  with diffusion into sphere                                                 \n\
  Input parameters include:                                                  \n\
    -length                      : column length, cm                         \n\
    -diameter                    : column diameter, cm                       \n\
    -velocity                    : cm/s                                      \n\
    -dispersivity                : cm                                        \n\
    -diffusioncoefficient        : Dm, cm2/s                                 \n\
    -tortuosity                  :                                           \n\
    -filmthickness               : cm                                        \n\
    -inletconcentration          : inlet concentration                       \n\
    -initialconcentrationcolumn  : initial concentration                     \n\
                                                                               \
    -radiusresin                 : cm                                        \n\
    -volumeresin                 : resin volume, cm^3                        \n\
    -watercontentresin           : resin water content                       \n\
    -kp                          : partition coefficient c/s                 \n\
                                                                               \
    -nx                          : number of cells in flow direction         \n\
    -nr                          : number of cells in the sphere for calc.   \n\
    -nt                          : number of time steps                      \n\
    -noutput                     : number of outputs for all concentrations  \n\
                                                                               \
    -cfl                         : Courant-Friedichs-Lewy (for accuracy)     \n\
    -simulationduration          : s                                         \n\
    -equalvolume                 : for sphere discretization                 \n\
    -displaymatrix               :                                           \n\
    -displayeta                  :                                           \n\
    -echo                        :                                           \n\
    -correctnumericaldiffusion   :                                        \n\n"; 

#include <petscksp.h>
#include <petscsys.h>
#include <petscviewerhdf5.h>

#undef __FUNCT__
#define __FUNCT__ "main"
int main(int argc,char **args)
{
  /*--------------------------------------------------------------------------*/
  /* column parameters */
  PetscReal length        = 15.0;     /* column length, cm     */
  PetscReal diameter      = 1.1;      /* column diameter, cm   */
  PetscReal dispersivity  = 0.1;      /* cm                    */
  PetscReal theta_m;                  /* mobile porosity for column           */
  PetscReal velocity      = 0.077;    /* cm/s 0.2 bv/min = 0.2 * 15/60/0.65   */

  /* resin parameters */
  PetscReal volume_resin  = 5.0;      /* resin volume, ml      */
  PetscReal radius_resin  = 0.028;    /* sphere radius, cm     */
  PetscReal theta_resin   = 0.50;     /* volumetric water content for resin  */

  PetscReal diffusioncoef = 1.0e-5;   /* Dm, molecular diffusion coef cm2/s */
  PetscReal tortuosity   = 0.2;       /* De/Dm in sphere                     */
  PetscReal filmthickness = 1.0e-3;   /* film thickness                      */
  PetscReal kp            = 2.5e5;    /* partition coefficient */
  PetscReal cin           = 1.0;      /* inlet concentration M */
  PetscReal cinit         = 0.0;      /* initial concentration in column  */

  PetscReal cfl_limiter   = 0.2;      /* limit cfl to determine dt for accuracy */        
  PetscReal sim_duration  = 900.0; 

  PetscBool correct_nd = PETSC_FALSE; /* correct numerical diffusion */ 
  PetscBool equalvolume  = PETSC_FALSE;
  PetscBool displaymatrix = PETSC_FALSE;
  PetscBool displayeta = PETSC_FALSE;
  PetscBool echo = PETSC_TRUE;

  PetscInt  nx = 100, nr = 100, nt = 900; /* number of cells in column, and sphere */          
  PetscReal dx, dr, dt, tt;           /*  */
  PetscReal r, area, dvol, r3n, volb;

  PetscReal num_bead_cell;            /* number of beads in each column cell   */

  PetscReal tmpreal, val[4];

  PetscReal alpha, beta, gamma;
  PetscReal Pe, DnDe;

  PetscInt i, j, col[4];

  PetscErrorCode ierr;
  Vec            eta;
  Vec            sphere_cell_dvol;
  Vec            x,b;
  Vec            cbt; /* breakthrough concentration */
  PetscScalar    *eta_p, *vol_p;
  Mat            A;       
  KSP            ksp;    
  PetscInt       Ii,Istart,Iend;

  PetscViewer    viewer;
  PetscBool      flg;
  char str[80], filename[80], prefix[80]="base";
  char restart_filename[80]="";
  PetscInt       iout, nto, ntout=10;
  PetscInt       iobs[10];
  PetscScalar    cobs[10];

  PetscInitialize(&argc,&args,(char*)0,help);

  ierr = PetscOptionsGetString(NULL, "-prefix", prefix, sizeof(prefix), &flg); CHKERRQ(ierr);
  ierr = PetscOptionsGetString(NULL, "-restart", restart_filename, sizeof(restart_filename), &flg); CHKERRQ(ierr);

  ierr = PetscOptionsGetReal(NULL,"-length", &length, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-diameter", &diameter, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-dispersivity", &dispersivity, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-diffusioncoefficient", &diffusioncoef, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-tortuosity", &tortuosity, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-filmthickness", &filmthickness, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-inletconcentration", &cin, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-initialconcentration", &cinit, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-kp", &kp, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-velocity", &velocity, NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetReal(NULL,"-radiusresin", &radius_resin, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-volumeresin", &volume_resin, NULL); CHKERRQ(ierr);
  ierr = PetscOptionsGetReal(NULL,"-watercontentresin", &theta_resin, NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetInt(NULL, "-nx", &nx, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL, "-nr", &nr, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL, "-nt", &nt, NULL);CHKERRQ(ierr);
  ierr = PetscOptionsGetInt(NULL, "-noutput", &ntout, NULL);CHKERRQ(ierr);

  ierr = PetscOptionsGetReal(NULL,"-cfl", &cfl_limiter, NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetReal(NULL,"-simulationduration",&sim_duration,NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetBool(NULL,"-correctnumericaldiffusion",&correct_nd,NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetBool(NULL,"-equalvolume",&equalvolume,NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetBool(NULL,"-displaymatrix",&displaymatrix,NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetBool(NULL,"-displayeta",&displayeta,NULL); CHKERRQ(ierr);

  ierr = PetscOptionsGetBool(NULL,"-echo",&echo,NULL); CHKERRQ(ierr);

  if (echo) {
    PetscPrintf(PETSC_COMM_WORLD, "Length     = %10.3f cm\n", length);
    PetscPrintf(PETSC_COMM_WORLD, "Radius     = %10.6f cm\n", radius_resin);
    PetscPrintf(PETSC_COMM_WORLD, "Velocity   = %10.3f cm/s\n", velocity);
    PetscPrintf(PETSC_COMM_WORLD, "Dm         = %10.3e cm2/s\n", diffusioncoef);
    PetscPrintf(PETSC_COMM_WORLD, "Tortuosity = %10.3f \n", tortuosity);
    PetscPrintf(PETSC_COMM_WORLD, "Dispersivity = %10.3f \n", dispersivity);
    PetscPrintf(PETSC_COMM_WORLD, "Filmthickness = %10.3e \n", filmthickness);
    PetscPrintf(PETSC_COMM_WORLD, "Partitioncoef = %10.3e \n", kp);
  }

  dx    = length/((float)nx);

  theta_m = 1.0 - volume_resin / (PETSC_PI * diameter * diameter / 4.0 * length); 

  dt = sim_duration / ((float)nt);

  alpha = velocity * dt / dx; 

  Pe    = velocity * dx / (diffusioncoef * tortuosity + velocity * dispersivity);
  DnDe  = 0.5 * Pe * (1.0 + alpha); 

  PetscPrintf(PETSC_COMM_WORLD, "Pe = %10.3f\n", Pe);
  PetscPrintf(PETSC_COMM_WORLD, "Cr = %10.3f\n", alpha);
  PetscPrintf(PETSC_COMM_WORLD, "Dn/De = %10.3f\n", DnDe);

  beta  = (diffusioncoef * tortuosity + dispersivity * velocity) * dt / dx / dx;

  if (correct_nd) beta = beta * (1.0 - DnDe);

  /*
  if (beta < 0.0) {
    ierr = SETERRQ(PETSC_COMM_WORLD, 1, "beta  < 0!, too big numerical dispersion!"); CHKERRQ(ierr);
  } 
  */ 
  ierr = VecCreateSeq(PETSC_COMM_SELF, nr+1, &eta); CHKERRQ(ierr);
  ierr = VecSetFromOptions(eta);
  ierr = VecDuplicate(eta, &sphere_cell_dvol); CHKERRQ(ierr);

  r = radius_resin;
  num_bead_cell = volume_resin / (4.0 * PETSC_PI * r*r*r / 3.0) / ((float)nx);  

  /* film cell */
  volb = 4.0 * PETSC_PI * r * r * r / 3.0;
  r = radius_resin + filmthickness;
  dvol = 4.0 * PETSC_PI * r * r * r / 3.0 - volb;
  ierr = VecSetValue(sphere_cell_dvol, 0, dvol, INSERT_VALUES); CHKERRQ(ierr);

  area = 4.0 * PETSC_PI * r * r;
  tmpreal = diffusioncoef * dt * area / (0.5 * filmthickness);
  ierr = VecSetValue(eta, 0, tmpreal, INSERT_VALUES); CHKERRQ(ierr);
  gamma = tmpreal * num_bead_cell / theta_m / \
      (PETSC_PI * diameter * diameter / 4.0 * length / ((float)nx));

  r = radius_resin;
  area = 4.0 * PETSC_PI * r * r;

  /* for cells in the sphere */
  if (equalvolume) {
    dvol = 4.0 * PETSC_PI * r * r * r / 3.0 / ((float)nr);
    for (i = 0; i < nr; i++) {
      ierr = VecSetValue(sphere_cell_dvol, i+1, dvol, INSERT_VALUES); CHKERRQ(ierr);
    }
 
    dr = r - r * pow((1.0 - 1.0 / ((float)nr)), 1.0/3.0);
    tmpreal = diffusioncoef * tortuosity * dt * area / (0.5 * (filmthickness + dr));
    ierr = VecSetValue(eta, 1, tmpreal, INSERT_VALUES); CHKERRQ(ierr);

    r3n = r * r * r / ((float)nr);

    for (i = 0; i < nr - 1; i++)
    {
      r = r - dr;
      area = 4.0 * PETSC_PI * r * r;
      tmpreal = dr;
      dr = r - pow(r * r * r - r3n, 1.0/3.0);
      tmpreal = diffusioncoef * tortuosity * dt * area / (dr + tmpreal) * 2.0;
      ierr = VecSetValue(eta, i+2, tmpreal, INSERT_VALUES); CHKERRQ(ierr);
    }
  } else {
    dr = r / ((float)nr);
    dvol = 4.0 * PETSC_PI * (r * r * r - (r - dr) * (r - dr) * (r - dr)) / 3.0;
    tmpreal = diffusioncoef * tortuosity * dt * area / (0.5 * (filmthickness + dr));

    ierr = VecSetValue(eta, 1, tmpreal, INSERT_VALUES); CHKERRQ(ierr);
    ierr = VecSetValue(sphere_cell_dvol, 1, dvol, INSERT_VALUES); CHKERRQ(ierr);

    for (i = 0; i < nr - 1; i++)
    {
      r = r - dr;
      area = 4.0 * PETSC_PI * r * r;        
      dvol = 4.0 * PETSC_PI * (r * r * r - (r - dr) * (r - dr) * (r - dr)) / 3.0;
      tmpreal = diffusioncoef * tortuosity * dt * area / dr;
      ierr = VecSetValue(eta, i+2, tmpreal, INSERT_VALUES); CHKERRQ(ierr);
      ierr = VecSetValue(sphere_cell_dvol, i+2, dvol, INSERT_VALUES); CHKERRQ(ierr);
    }
  }

  ierr = VecAssemblyBegin(eta); CHKERRQ(ierr);
  ierr = VecAssemblyBegin(sphere_cell_dvol); CHKERRQ(ierr);
  ierr = VecAssemblyEnd(eta); CHKERRQ(ierr);
  ierr = VecAssemblyEnd(sphere_cell_dvol); CHKERRQ(ierr);

  if (displayeta) { 
    ierr = PetscPrintf(PETSC_COMM_WORLD, "alpha = %10.3e\n", alpha); CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "beta = %10.3e\n", beta); CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "gamma = %10.3e\n", gamma); CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD, "eta = \n"); CHKERRQ(ierr);
    ierr = VecView(eta,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
    ierr = VecView(sphere_cell_dvol,PETSC_VIEWER_STDOUT_WORLD);CHKERRQ(ierr);
  }
  /* now that alpha, beta, gamma, and eta are ready for matrix assembly */

  ierr = MatCreate(PETSC_COMM_WORLD,&A);CHKERRQ(ierr);
  ierr = MatSetSizes(A,PETSC_DECIDE,PETSC_DECIDE,nx*(nr+2),nx*(nr+2));CHKERRQ(ierr);
  ierr = MatSetFromOptions(A);CHKERRQ(ierr);
  ierr = MatMPIAIJSetPreallocation(A,4,NULL,4,NULL);CHKERRQ(ierr);
  ierr = MatSeqAIJSetPreallocation(A,4,NULL);CHKERRQ(ierr);
  ierr = MatSetUp(A);CHKERRQ(ierr);

  ierr = MatGetOwnershipRange(A,&Istart,&Iend);CHKERRQ(ierr);

  if (Iend >= nx) {
    ierr = VecGetArray(eta, &eta_p);CHKERRQ(ierr);
    ierr = VecGetArray(sphere_cell_dvol, &vol_p);CHKERRQ(ierr); 
  }
                
  for (Ii=Istart; Ii<Iend; Ii++) 
  {
    if (Ii == 0) {
      /* first row */
      col[0] = 0;
      col[1] = 1;
      col[2] = nx;
      val[0] = 1.0 + alpha + 2.0 * beta + gamma;
      val[1] = -1.0 * beta;
      val[2] = -1.0 * gamma;
      ierr = MatSetValues(A, 1, &Ii, 3, col, val, INSERT_VALUES);CHKERRQ(ierr); 
    } 
    else if (Ii > 0 && Ii < nx-1) { 
      /* second to second last row  for the mobile cells in the column */
      col[0] = Ii - 1;
      col[1] = Ii;
      col[2] = Ii + 1;
      col[3] = Ii + nx;
      val[0] = -1.0 * (alpha + beta);
      val[1] = 1.0 + alpha + 2.0 * beta + gamma;
      val[2] = -1.0 * beta;
      val[3] = -1.0 * gamma;
      ierr = MatSetValues(A, 1, &Ii, 4, col, val, INSERT_VALUES);CHKERRQ(ierr); 
    }
    else if (Ii == nx-1){
      /* last row in the column */
      col[0] = nx - 2;
      col[1] = nx - 1;
      col[2] = 2 * nx - 1;
      val[0] = -1.0 * (alpha + beta);
      val[1] = 1.0 + alpha + beta + gamma;
      val[2] = -1.0 * gamma;
      ierr = MatSetValues(A, 1, &Ii, 3, col, val, INSERT_VALUES);CHKERRQ(ierr); 
    }
    else if (Ii >= nx && Ii < 2 * nx) {
      /* film cells */
      j = Ii / nx - 1; 
      col[0] = Ii - nx;
      col[1] = Ii;
      col[2] = Ii + nx;
      val[0] = -1.0 * eta_p[j] / vol_p[j];
      val[2] = -1.0 * eta_p[j+1] / vol_p[j];
      val[1] = 1.0 - val[0] - val[2];
      ierr = MatSetValues(A, 1, &Ii, 3, col, val, INSERT_VALUES);CHKERRQ(ierr); 
    }
    else if (Ii >= 2 * nx && Ii < nx * (nr+1)) {
      /* sphere cells */
      j = Ii / nx; /* - 1; */
      col[0] = Ii - nx;
      col[1] = Ii;
      col[2] = Ii + nx;
      val[0] = -1.0 * eta_p[j] / vol_p[j] / (1.0 + kp);
      val[2] = -1.0 * eta_p[j+1] / vol_p[j] / (1.0 + kp);
      val[1] = 1.0 - val[0] - val[2];
      ierr = MatSetValues(A, 1, &Ii, 3, col, val, INSERT_VALUES);CHKERRQ(ierr); 
    }
    else if(Ii >= nx*(nr+1)) {
      /* grid cells in the center of the sphere */
      j = nr; /* - 1; */
      col[0] = Ii - nx;
      col[1] = Ii;
      val[0] = -1.0 * eta_p[j] / vol_p[j] / (1.0 + kp);
      val[1] = 1.0 - val[0];
      ierr = MatSetValues(A, 1, &Ii, 2, col, val, INSERT_VALUES);CHKERRQ(ierr); 
    } 
  }

  if (Iend >= nx ) {
    ierr = VecRestoreArray(eta, &eta_p);CHKERRQ(ierr);
    ierr = VecRestoreArray(sphere_cell_dvol, &vol_p);CHKERRQ(ierr); 
  }

  ierr = MatAssemblyBegin(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);
  ierr = MatAssemblyEnd(A,MAT_FINAL_ASSEMBLY);CHKERRQ(ierr);

  if (displaymatrix) {
    ierr = MatView(A,PETSC_VIEWER_STDOUT_WORLD); CHKERRQ(ierr);
  }

  /* now setup the vector */
  ierr = VecCreate(PETSC_COMM_WORLD,&b);CHKERRQ(ierr);
  ierr = VecSetSizes(b,PETSC_DECIDE,nx*(nr+2));CHKERRQ(ierr);
  ierr = VecSetFromOptions(b);CHKERRQ(ierr);

  if (restart_filename[0] == '\0') { 
/*    ierr = VecSet(b,0.0);CHKERRQ(ierr); */
    ierr = VecGetOwnershipRange(b, &Istart, &Iend); CHKERRQ(ierr);
    for (i = Istart; i < Iend; i++) {
      if (i < nx) {
        ierr = VecSetValue(b, i, cinit, ADD_VALUES); CHKERRQ(ierr);
      } 
      else {
        ierr = VecSetValue(b, i, 0.0, ADD_VALUES); CHKERRQ(ierr);
      }
    }
  }
  else
  {
    ierr = PetscObjectSetName((PetscObject) b, "restart");CHKERRQ(ierr);
    ierr = PetscPrintf(PETSC_COMM_WORLD,"Restart from %s\n", restart_filename); CHKERRQ(ierr);
    ierr = PetscViewerHDF5Open(PETSC_COMM_WORLD,restart_filename,FILE_MODE_READ,&viewer);  CHKERRQ(ierr);   
    
    ierr = VecLoad(b, viewer);  CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
  }

  ierr = VecDuplicate(b,&x);CHKERRQ(ierr);

  /* Create the HDF5 viewer for writing */
  sprintf(filename, "%s_%dx%dx%d.h5", prefix, nx, nr, nt);
  ierr = PetscViewerHDF5Open(PETSC_COMM_WORLD,filename,FILE_MODE_WRITE,&viewer); CHKERRQ(ierr);
  ierr = PetscViewerSetFromOptions(viewer); CHKERRQ(ierr);

  /* set up solver */
  ierr = KSPCreate(PETSC_COMM_WORLD,&ksp);CHKERRQ(ierr);
  ierr = KSPSetOperators(ksp,A,A);CHKERRQ(ierr);
  ierr = KSPSetFromOptions(ksp);CHKERRQ(ierr);

  tt = 0.0;
  nt = (int)(sim_duration / dt) + 1;

  if (ntout > nt) nto = 1;
  else if (ntout <= 0) nto = -1;
  else nto = nt / ntout; 

  /* set observation output, currently breakthrough curve */
  iobs[0] = nx - 1;

  ierr = VecGetOwnershipRange(x, &Istart, &Iend); CHKERRQ(ierr);
  if (Istart < iobs[0] && Iend > iobs[0]) {
    ierr = VecCreateSeq(PETSC_COMM_SELF, nt+1, &cbt); CHKERRQ(ierr);
    ierr = VecSetFromOptions(cbt);
    ierr = VecSet(cbt, 0.0); CHKERRQ(ierr);
  }

  iout = 0;  
  
  for (i = 0; i <= nt; i++){
    ierr = VecGetOwnershipRange(b, &Istart, &Iend); CHKERRQ(ierr);
    if (Istart < 1) {
      tmpreal = (alpha + beta) * cin;
      ierr = VecSetValue(b, 0, tmpreal, ADD_VALUES); CHKERRQ(ierr);
    }
    ierr = VecAssemblyBegin(b); CHKERRQ(ierr);
    ierr = VecAssemblyEnd(b); CHKERRQ(ierr);
    ierr = KSPSolve(ksp,b,x);CHKERRQ(ierr);

    ierr = VecGetOwnershipRange(x, &Istart, &Iend); CHKERRQ(ierr);

    if (Istart < iobs[0] && Iend > iobs[0]) {
      ierr = VecGetValues(x, 1, iobs, cobs); CHKERRQ(ierr); 
      ierr = VecSetValue(cbt, i, cobs[0], INSERT_VALUES); CHKERRQ(ierr);
    }

    iout = iout + 1;
    tt = tt + dt;

    if (iout == nto) { 
      sprintf(str, "t = %10.5f", tt);
      ierr = PetscObjectSetName((PetscObject) x, str); CHKERRQ(ierr);
      iout = 0;
      ierr = VecView(x,viewer); CHKERRQ(ierr);
      ierr = PetscPrintf(PETSC_COMM_WORLD,"t = %10.5f\n", tt); CHKERRQ(ierr);
    }

    ierr = VecCopy(x, b);
  }

  ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);

  /* write restart file */
  sprintf(filename, "%s_%dx%d_restart.h5", prefix, nx, nr);
  ierr = PetscObjectSetName((PetscObject) x, "restart"); CHKERRQ(ierr);
  ierr = PetscViewerHDF5Open(PETSC_COMM_WORLD,filename,FILE_MODE_WRITE,&viewer); CHKERRQ(ierr);
  ierr = PetscViewerSetFromOptions(viewer); CHKERRQ(ierr);
  ierr = VecView(x,viewer); CHKERRQ(ierr);
  ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);

  /* write breakthrough /observation file */

  ierr = VecGetOwnershipRange(x, &Istart, &Iend); CHKERRQ(ierr);
  if (Istart < iobs[0] && Iend > iobs[0]) {
    ierr = VecAssemblyBegin(cbt); CHKERRQ(ierr);
    ierr = VecAssemblyEnd(cbt); CHKERRQ(ierr);
    sprintf(filename, "%s_%dx%dx%d_btc.h5", prefix, nx, nr, nt);
    ierr = PetscObjectSetName((PetscObject) cbt, "btc"); CHKERRQ(ierr);
    ierr = PetscViewerHDF5Open(PETSC_COMM_SELF,filename,FILE_MODE_WRITE,&viewer); CHKERRQ(ierr);
    ierr = PetscViewerSetFromOptions(viewer); CHKERRQ(ierr);
    ierr = VecView(cbt,viewer); CHKERRQ(ierr);
    ierr = PetscViewerDestroy(&viewer); CHKERRQ(ierr);
    ierr = VecDestroy(&cbt);CHKERRQ(ierr);
  }

  ierr = KSPDestroy(&ksp);CHKERRQ(ierr);
  ierr = VecDestroy(&eta); CHKERRQ(ierr);
  ierr = VecDestroy(&sphere_cell_dvol); CHKERRQ(ierr);
  ierr = VecDestroy(&x);CHKERRQ(ierr);
  ierr = VecDestroy(&b);CHKERRQ(ierr);
  ierr = MatDestroy(&A);CHKERRQ(ierr);
  ierr = PetscFinalize();

  return 0;
}

