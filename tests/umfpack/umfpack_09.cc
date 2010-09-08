//----------------------------------------------------------------------
//    $Id$
//    Version: $Name$
//
//    Copyright (C) 2002, 2003, 2004, 2005, 2006, 2009, 2010 by the deal.II authors
//
//    This file is subject to QPL and may not be  distributed
//    without copyright and license information. Please refer
//    to the file deal.II/doc/license.html for the  text  and
//    further information on this license.
//
//----------------------------------------------------------------------

// test the umfpack sparse direct solver on a mass matrix that is
// slightly modified to make it nonsymmetric. same as umfpack_03, but
// for a BlockSparseMatrix<double> instead of SparseMatrix<double> matrix

#include "../tests.h"
#include <iomanip>
#include <fstream>
#include <cstdlib>

#include <base/quadrature_lib.h>
#include <base/function.h>

#include <fe/fe_q.h>
#include <fe/fe_values.h>

#include <dofs/dof_tools.h>

#include <lac/block_sparse_matrix.h>
#include <lac/block_sparsity_pattern.h>
#include <lac/vector.h>
#include <lac/sparse_direct.h>

#include <grid/tria.h>
#include <grid/grid_generator.h>

#include <numerics/vectors.h>
#include <numerics/matrices.h>


template <int dim>
void test ()
{
  deallog << dim << 'd' << std::endl;

  Triangulation<dim> tria;
  GridGenerator::hyper_cube (tria,0,1);
  tria.refine_global (1);

                                   // destroy the uniformity of the matrix by
                                   // refining one cell
  tria.begin_active()->set_refine_flag ();
  tria.execute_coarsening_and_refinement ();
  tria.refine_global(8-2*dim);

  FE_Q<dim> fe (1);
  DoFHandler<dim> dof_handler (tria);
  dof_handler.distribute_dofs (fe);

  deallog << "Number of dofs = " << dof_handler.n_dofs() << std::endl;

  BlockSparsityPattern sparsity_pattern;
  sparsity_pattern.reinit (3, 3);
  for (unsigned int i=0; i<3; ++i)
    for (unsigned int j=0; j<3; ++j)
      sparsity_pattern.block(i,j).reinit (i!=2 ?
					  dof_handler.n_dofs()/3 :
					  dof_handler.n_dofs()-2*(dof_handler.n_dofs()/3),
					  j!=2 ?
					  dof_handler.n_dofs()/3 :
					  dof_handler.n_dofs()-2*(dof_handler.n_dofs()/3),
					  dof_handler.max_couplings_between_dofs(),
					  false);
  sparsity_pattern.collect_sizes();
  DoFTools::make_sparsity_pattern (dof_handler, sparsity_pattern);
  sparsity_pattern.compress ();

  BlockSparseMatrix<double> B;
  B.reinit (sparsity_pattern);

  {
				     // for some reason, we can't
				     // create block matrices directly
				     // in matrixtools...
    SparsityPattern xsparsity_pattern;
    xsparsity_pattern.reinit (dof_handler.n_dofs(),
			      dof_handler.n_dofs(),
			      dof_handler.max_couplings_between_dofs());
    DoFTools::make_sparsity_pattern (dof_handler, xsparsity_pattern);
    xsparsity_pattern.compress ();

    SparseMatrix<double> xB;
    xB.reinit (xsparsity_pattern);

    QGauss<dim> qr (2);
    MatrixTools::create_mass_matrix (dof_handler, qr, xB);

				     // scale lower left part of the matrix by
				     // 1/2 and upper right part by 2 to make
				     // matrix nonsymmetric
    for (SparseMatrix<double>::iterator p=xB.begin();
	 p!=xB.end(); ++p)
      if (p->column() < p->row())
	p->value() = p->value()/2;
      else if (p->column() > p->row())
	p->value() = p->value() * 2;

				     // check that we've done it right
    for (SparseMatrix<double>::iterator p=xB.begin();
	 p!=xB.end(); ++p)
      if (p->column() != p->row())
	Assert (xB(p->row(),p->column()) != xB(p->column(),p->row()),
		ExcInternalError());

				     // now copy stuff over
    for (SparseMatrix<double>::const_iterator i = xB.begin(); i != xB.end(); ++i)
      B.set (i->row(), i->column(), i->value());
  }


                                   // for a number of different solution
                                   // vectors, make up a matching rhs vector
                                   // and check what the UMFPACK solver finds
  for (unsigned int i=0; i<3; ++i)
    {
      BlockVector<double> solution (3);
      solution.block(0).reinit(dof_handler.n_dofs()/3);
      solution.block(1).reinit(dof_handler.n_dofs()/3);
      solution.block(2).reinit(dof_handler.n_dofs()-2*(dof_handler.n_dofs()/3));
      solution.collect_sizes();
      BlockVector<double> x;
      x.reinit (solution);
      BlockVector<double> b;
      b.reinit (solution);

      for (unsigned int j=0; j<dof_handler.n_dofs(); ++j)
        solution(j) = j+j*(i+1)*(i+1);

      B.vmult (b, solution);
      x = b;
      Vector<double> tmp (solution.size());
      tmp = x;
      SparseDirectUMFPACK().solve (B, tmp);
      x = tmp;

      x -= solution;
      deallog << "relative norm distance = "
              << x.l2_norm() / solution.l2_norm()
              << std::endl;
      deallog << "absolute norms = "
              << x.l2_norm() << ' ' << solution.l2_norm()
              << std::endl;
      Assert (x.l2_norm() / solution.l2_norm() < 1e-8,
              ExcInternalError());
    }
}


int main ()
{
  std::ofstream logfile("umfpack_09/output");
  deallog.attach(logfile);
  deallog.depth_console(0);
  deallog.threshold_double(1.e-8);

  test<1> ();
  test<2> ();
  test<3> ();
}
