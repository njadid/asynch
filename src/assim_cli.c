#if !defined(_MSC_VER)
#include <config.h>
#else 
#include <config_msvc.h>
#endif

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <mpi.h>

#if defined(HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined(HAVE_POSTGRESQL)
#include <libpq-fe.h>
#endif

#if defined(HAVE_PETSC)
#include <petsc.h>
#endif

#if !defined(_MSC_VER)
#define ASYNCH_SLEEP(seconds) sleep(seconds)
#else
#include <windows.h>
#define ASYNCH_SLEEP(seconds) Sleep((seconds) * 1000)
#endif

#include <asynch_interface.h>
#include <assim/linear_least_squares.h>
#include <assim/models.h>
#include <assim/ancillary.h>
#include <optparse.h>

// Global variables
bool verbose = false;
int my_rank;
int np;

////Output functions
//int Output_Linkid(double t, VEC y_i, VEC global_params, VEC params, int state, void* user);
//void Set_Output_User_LinkID(asynchsolver* asynch);
//
//int Output_Timestamp(double t, VEC y_i, VEC global_params, VEC params, int state, void* user);
//void Set_Output_User_Timestamp(asynchsolver* asynch);


//Print to stdout only for process of rank 0
int print_out(const char* format, ...)
{
    int res = 0;
    if (my_rank == 0)
    {
        va_list args;
        va_start(args, format);
        res = vprintf(format, args);
        va_end(args);
    }

    return res;
}


//Print to stderr only for process of rank 0
int print_err(const char* format, ...)
{
    int res = 0;
    if (my_rank == 0)
    {
        va_list args;
        va_start(args, format);
        res = vfprintf(stderr, format, args);
        va_end(args);
    }

    return res;
}


//Make sure we finalize MPI
void asynch_onexit(void)
{
    int flag;
    MPI_Finalized(&flag);
    if (!flag)
        MPI_Finalize();
}


int main(int argc, char* argv[])
{
    int res;

    //Initialize MPI stuff
    res = MPI_Init(&argc, &argv);
    if (res == MPI_SUCCESS)
        atexit(asynch_onexit);
    else
    {
        print_err("Failed to initialize MPI");
        exit(EXIT_FAILURE);
    }

    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &np);

    //PETSC
    PetscInitialize(&argc, &argv, NULL, NULL);

    //Command line options
    bool debug = false;
    bool help = false;
    bool version = false;

    //Parse command line
    struct optparse options;
    optparse_init(&options, argv);
    struct optparse_long longopts[] = {
        { "debug", 'd', OPTPARSE_NONE },
        { "help", 'h', OPTPARSE_NONE },
        { "version", 'v', OPTPARSE_NONE },
        { "verbose", 'w', OPTPARSE_NONE },
        { 0 }
    };
    int option;
    while ((option = optparse_long(&options, longopts, NULL)) != -1) {
        switch (option) {
        case 'd':
            debug = true;
            break;
        case 'h':
            help = true;
            break;
        case 'v':
            version = true;
            break;
        case 'w':
            verbose = true;
            break;
        case '?':
            print_err("%s: %s\n", argv[0], options.errmsg);
            exit(EXIT_FAILURE);
        }
    }

    if (version) print_out("This is %s\n", PACKAGE_STRING);
    if (help)
    {
        print_out("Usage: assim <global file> <das file>\n", PACKAGE_STRING);
        print_out(
            "  -d [--debug]   : Wait for the user input at the begining of the program (useful" \
            "                   for attaching a debugger)\n" \
            "  -w [--verbose] : Print debugging information to stdout\n" \
            "  -v [--version] : Print the current version of ASYNCH\n");
        exit(EXIT_SUCCESS);
    }
    if (version || help) exit(EXIT_SUCCESS);

    //Parse remaining arguments
    char *global_filename = optparse_arg(&options);
    if (global_filename == NULL && my_rank == 0)
    {
        print_err("Command line parameter required:  A universal variable file (.gbl).\n");
        exit(EXIT_FAILURE);
    }

    char *assim_filename = optparse_arg(&options);
    if (assim_filename == NULL && my_rank == 0)
    {
        print_err("Command line parameter required:  An assim file (.das).\n");
        exit(EXIT_FAILURE);
    }

    //Disable stdout buffering
    setvbuf(stdout, NULL, _IONBF, 0);

    if (debug)
    {
        //Disable stdout buffering
        setvbuf(stdout, NULL, _IONBF, 0);

        //When the program first starts to execute, at the very beginning of our program, we 
        //ask the user to type some sort of input to simply stall the application until start your
        //"Attach to Process" and you can attach to all the different threads in your program.
        if (my_rank == 0)
        {
            printf("You may now attach the debugger then press enter.\n");
            //fflush(stdout);
            int ch = getchar();
        }

        MPI_Barrier(MPI_COMM_WORLD); // All threads will wait here until you give thread 0 an input
    }

    //Declare variables
    unsigned int i, j/*, k,l,m,n,*/;
    //RKMethod** AllMethods;
    //char additional[16];	//For output filename
    //srand(time(NULL));	//!!!! Is this needed? !!!!

    //Init asynch object and the river network
    AsynchSolver *asynch = Asynch_Init(MPI_COMM_WORLD, verbose);

    //Create assim structure
    AssimData assim;
    //Read data assimilation file
    InitAssimData(&assim, assim_filename);

    //Model 254, full
    AsynchModel model_254_assim;
    memset(&model_254_assim, 0, sizeof(AsynchModel));
    model_254_assim.dim = 4;
    model_254_assim.set_param_sizes = SetParamSizes_Assim_254;
    model_254_assim.convert = ConvertParams_Assim_254;
    model_254_assim.routines = InitRoutines_Assim_254;
    model_254_assim.precalculations = Precalculations_Assim_254;
    model_254_assim.initialize_eqs = ReadInitData_Assim_254;

    //Model 254, q
    AsynchModel model_254_assim_q;
    memset(&model_254_assim_q, 0, sizeof(AsynchModel));
    model_254_assim_q.dim = 4;
    model_254_assim_q.set_param_sizes = SetParamSizes_Assim_254;
    model_254_assim_q.convert = ConvertParams_Assim_254;
    model_254_assim_q.routines = InitRoutines_Assim_254_q;
    model_254_assim_q.precalculations = Precalculations_Assim_254;
    model_254_assim_q.initialize_eqs = ReadInitData_Assim_254_q;

    //Model 254, q and s_p
    AsynchModel model_254_assim_qsp;
    memset(&model_254_assim_qsp, 0, sizeof(AsynchModel));
    model_254_assim_qsp.dim = 4;
    model_254_assim_qsp.set_param_sizes = SetParamSizes_Assim_254;
    model_254_assim_qsp.convert = ConvertParams_Assim_254;
    model_254_assim_qsp.routines = InitRoutines_Assim_254_qsp;
    model_254_assim_qsp.precalculations = Precalculations_Assim_254;
    model_254_assim_qsp.initialize_eqs = ReadInitData_Assim_254_qsp;

    //Model 254, q and s_t
    AsynchModel model_254_assim_qst;
    memset(&model_254_assim_qst, 0, sizeof(AsynchModel));
    model_254_assim_qst.dim = 4;
    model_254_assim_qst.set_param_sizes = SetParamSizes_Assim_254;
    model_254_assim_qst.convert = ConvertParams_Assim_254;
    model_254_assim_qst.routines = InitRoutines_Assim_254_qst;
    model_254_assim_qst.precalculations = Precalculations_Assim_254;
    model_254_assim_qst.initialize_eqs = ReadInitData_Assim_254_qst;

    if (strcmp(assim.model, "254") == 0)
    {
        Asynch_Custom_Model(asynch, &model_254_assim);
    }
    else if (strcmp(assim.model, "254_q") == 0)
    {
        Asynch_Custom_Model(asynch, &model_254_assim_q);
    }
    else if (strcmp(assim.model, "254_qsp") == 0)
    {
        Asynch_Custom_Model(asynch, &model_254_assim_qsp);
    }
    else if (strcmp(assim.model, "254_qst") == 0)
    {
        Asynch_Custom_Model(asynch, &model_254_assim_qst);
    }
    else /* default: */
    {
        print_err("Invalid model variant (expected 254, 254_q, 254_qsp or 254_qst an got %s", assim.model);
        MPI_Abort(MPI_COMM_WORLD, 0);
    }

    //Model 15
    //Asynch_Custom_Model(asynch,&SetParamSizes_Assim,&ConvertParams_Assim,&InitRoutines_Assim,&Precalculations_Assim,&ReadInitData_Assim);
    //Model 254
    //Asynch_Custom_Model(asynch,&SetParamSizes_Assim_254,&ConvertParams_Assim_254,&InitRoutines_Assim_254,&Precalculations_Assim_254,&ReadInitData_Assim_254);
    //Model 254, q
    //Asynch_Custom_Model(asynch, &SetParamSizes_Assim_254, &ConvertParams_Assim_254, &InitRoutines_Assim_254_q,&Precalculations_Assim_254, &ReadInitData_Assim_254_q);
    //Model 254, q and s_p
    //Asynch_Custom_Model(asynch,&SetParamSizes_Assim_254,&ConvertParams_Assim_254,&InitRoutines_Assim_254_qsp,&Precalculations_Assim_254,&ReadInitData_Assim_254_qsp);
    //Model 254, q and s_t
    //Asynch_Custom_Model(asynch,&SetParamSizes_Assim_254,&ConvertParams_Assim_254,&InitRoutines_Assim_254_qst,&Precalculations_Assim_254,&ReadInitData_Assim_254_qst);
    //Asynch_Custom_Partitioning(asynch,&Partition_METIS_ByEqs);

    //Data assimilation parameters
/*
    unsigned int num_obs,*ids,*numsteps,*obs_locs;
    double forecast_window = 10.0*24*60;
    double inc = 15.0;
    unsigned int steps_to_use = 10;
    unsigned int least_squares_iters = 1;
*/
//For model 254
//unsigned int problem_dim = 7;	//!!!! Generalize this !!!!
//unsigned int assim_dim = 4;	//!!!! Generalize this !!!!
//For model 254 trim
    unsigned int problem_dim = 4;	//!!!! Generalize this !!!!
    unsigned int assim_dim = 4;	//!!!! Generalize this !!!!

    //Read global file
    if (my_rank == 0)	printf("Reading global file...\n");
    Asynch_Parse_GBL(asynch, global_filename);
    if (my_rank == 0)	printf("Loading network...\n");
    Asynch_Load_Network(asynch);

    //unsigned int obs_locs[] = {2};	//Locations of links with data	!!!! Make sure these are locations and not IDs !!!!
    //unsigned int obs_locs[] = {1,3,4};	//!!!! This should come from ReadSolution. Or should be calculated at least. !!!!
    //obs_locs = (unsigned int*) malloc(3*sizeof(unsigned int));
    //obs_locs[0] = 1; obs_locs[1] = 3; obs_locs[2] = 4;
    //double*** truesolution = ReadSolution("TempData/assim/yobservations.dat",asynch->id_to_loc,N,&num_obs,&ids,&obs_locs,&numsteps);
    //double*** truesolution = ReadSolution("TempData/assim/testobservations.dat",asynch->id_to_loc,asynch->N,&num_obs,&ids,&obs_locs,&numsteps);
    //double*** truesolution = ReadSolution("TempData/assim/254testobservations.dat",asynch->id_to_loc,asynch->N,&num_obs,&ids,&obs_locs,&numsteps);
    //double*** truesolution = DownloadGaugeReadings(1402790400,1405382400,asynch->id_to_loc,N,&num_obs,&ids,&obs_locs,&numsteps);	//June 15 2014 to July 15 2014
    //double*** truesolution = DownloadGaugeReadings(1403654400,1405382400,asynch->id_to_loc,asynch->N,&num_obs,&ids,&obs_locs,&numsteps);	//June 25 2014 to July 15 2014

    //Find the gauged locations
    if (GetObservationsIds(asynch, &assim))
        MPI_Abort(MPI_COMM_WORLD, 1);

    //Finds the link ids upstreams from every gauged locations
    const bool trim = true;
    FindUpstreamLinks2(asynch, &assim, problem_dim, trim, assim.obs_time_step, assim.num_steps, assim.obs_locs, assim.num_obs);

    print_out("Partitioning network...\n");
    Asynch_Partition_Network(asynch);
    //CleanUpstreamLinks(asynch);
    print_out("Loading parameters...\n");
    Asynch_Load_Network_Parameters(asynch);
    print_out("Reading dam and reservoir data...\n");
    Asynch_Load_Dams(asynch);
    print_out("Setting up numerical error data...\n");
    Asynch_Load_Numerical_Error_Data(asynch);
    print_out("Initializing model...\n");
    Asynch_Initialize_Model(asynch);
    Setup_Errors(asynch, problem_dim);
    print_out("Loading initial conditions...\n");
    Asynch_Load_Initial_Conditions(asynch);
    print_out("Loading forcings...\n");
    Asynch_Load_Forcings(asynch);
    print_out("Loading output data information...\n");
    Asynch_Load_Save_Lists(asynch);
    print_out("Finalizing network...\n");
    Asynch_Finalize_Network(asynch);
    print_out("Calculating initial step sizes...\n");
    Asynch_Calculate_Step_Sizes(asynch);

    // No output needed, since we are not forecasting

    ////Setup output for link id, if needed
    //int setup_id = Asynch_Check_Output(asynch, "LinkID");
    //int setup_timestamp = Asynch_Check_Output(asynch, "Timestamp");
    //if (setup_id || setup_timestamp)
    //{
    //    if (my_rank == 0)	printf("Error: forecaster needs LinkID (%i), Timestamp (%i).\n", setup_id, setup_timestamp);
    //    MPI_Abort(MPI_COMM_WORLD, 1);
    //}

    ////Setup output for link id, if needed
    //int setup = Asynch_Check_Output(asynch, "LinkID");
    //if (setup != -1)
    //{
    //    Set_Output_User_LinkID(asynch);
    //    Asynch_Set_Output(asynch, "LinkID", ASYNCH_INT, &Output_Linkid, NULL, 0);
    //}

    ////Setup output for timestamp, if needed
    //setup = Asynch_Check_Output(asynch, "Timestamp");
    //if (setup != -1)
    //{
    //    Asynch_Set_Output(asynch, "Timestamp", ASYNCH_INT, &Output_Timestamp, NULL, 0);
    //}

    ////Prepare output files
    //Asynch_Prepare_Temp_Files(asynch);
    //Asynch_Write_Current_Step(asynch);
    //Asynch_Prepare_Peakflow_Output(asynch);
    //Asynch_Prepare_Output(asynch);

    //Pull data from asynch
    Link **my_sys = asynch->my_sys;
    Lookup *id_to_loc = asynch->id_to_loc;
    unsigned int my_N = asynch->my_N, N = asynch->N, num_forcings = asynch->globals->num_forcings;
    int *assignments = asynch->assignments;
    Link* sys = asynch->sys;
    short int *getting = asynch->getting;
    GlobalVars *globals = asynch->globals;
    AsynchModel* custom_model = asynch->model;

    //Set print_time to t_0
    //Asynch_Reset_Temp_Files(asynch,sys[my_sys[0]]->last_t);

    ////Reserve space for backups
    //VEC* backup = (VEC*)malloc(N * sizeof(VEC));	//!!!! Same as background x_b? !!!!
    //for (i = 0; i < N; i++)
    //{
    //    if (assignments[i] == my_rank || getting[i])
    //        backup[i] = v_get(sys[i].dim);
    //    else
    //        backup[i] = v_get(0);
    //}

    //Initialize choices
    //unsigned int num_obs = assim.num_obs, *obs_locs = assim.obs_locs, num_steps = assim.num_steps;
    unsigned int num_total_obs = assim.num_steps * assim.num_obs;
    time_t begin_time = Asynch_Get_Begin_Timestamp(asynch);
    time_t end_time = Asynch_Get_End_Timestamp(asynch);
    double duration = Asynch_Get_Total_Simulation_Duration(asynch);
    double t_b = 0.0;
    unsigned int allstates = assim_dim * N;
    //double x_b[allstates];

    // Allocate background
    double  *x_b = calloc(allstates, sizeof(double));
    for (i = 0; i < N; i++)
    {
        if (assignments[i] == my_rank)
        {
            for (j = 0; j < assim_dim; j++)
                x_b[i*assim_dim + j] = sys[i].my->list.tail->y_approx[j];	//!!!! Need to be able to specify which states are used !!!!
        }
        //MPI_Bcast(&(x_b[i*assim_dim]), assim_dim, MPI_DOUBLE, assignments[i], MPI_COMM_WORLD);
    }

    int mpi_res = MPI_Allreduce(MPI_IN_PLACE, x_b, allstates, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    //Other initializations
    //unsigned int numstepstotake;
    //unsigned int iterations = (unsigned int)round((t_f - t_b) / inc);

    // Vector of observations (single step)
    double* d = calloc(assim.num_obs, sizeof(double));

    // Vector of observations (multiple steps)
    double* d_full = calloc(num_total_obs, sizeof(double));

    //Values used to start asynch solver in tao solvers
    double* x_start = calloc(allstates, sizeof(double));	//Values used to start asynch solver in tao solvers

    //Call model specific data assimilation routines
    //For Model 254
    //unsigned int allstates_needed = Setup_Fitting_Data_Model254(asynch,obs_locs,num_obs);
    //For Model 254 trim, q
    Setup_Fitting_Data_Model254_q(asynch, assim.obs_locs, assim.num_obs);
    //For Model 254 trim, q and s_p
    //Setup_Fitting_Data_Model254_qsp(asynch,obs_locs,num_obs);
    //For Model 254 trim, q and s_t
    //Setup_Fitting_Data_Model254_qst(asynch,obs_locs,num_obs);

    //Find locations unaffected by gauges
    unsigned int *vareq_shift, *inv_vareq_shift;
    unsigned int allstates_needed = BuildStateShift(asynch, allstates, assim.obs_locs, assim.num_obs, &vareq_shift, &inv_vareq_shift);


    printf("allstates_needed: %u allstates: %u\n", allstates_needed, allstates);

    //printf("!!!! Multiplied R by 10. Used 6 m/s (instead of 3). Only used 4 ls iterations. Using tolerance of 10 (instead of 30). Factor is 0.8.!!!!\n");

    /*
        //!!!! Assuming q and s_p are changing !!!!
        unsigned int curr_idx = 0;
        Vec B,R;
        if(my_rank == 0)
        {
            VecCreateSeq(MPI_COMM_SELF,allstates_needed,&B);
            for(i=0;i<N;i++)
            {
                if(links_needed[i])
                {
                    VecSetValue(B,curr_idx++,1.0,INSERT_VALUES);
                    VecSetValue(B,curr_idx++,1e2,INSERT_VALUES);
                }
            }
            VecAssemblyBegin(B);
            VecAssemblyEnd(B);

            VecCreateSeq(MPI_COMM_SELF,num_obs*steps_to_use,&R);
            for(i=0;i<num_obs;i++)
            {
                for(j=0;j<steps_to_use;j++)
                    VecSetValue(R,j*steps_to_use + i,invupareas[obs_locs[i]] * 10.0,INSERT_VALUES);
            }
            VecAssemblyBegin(R);
            VecAssemblyEnd(R);
        }
        free(links_needed);
    */


    //Prep PetSC
    if (my_rank == 0)
        printf("\nPrepping PetSc...\n");
    
    AssimWorkspace ws;

    //For linear least squares
    int *HM_col_indices = NULL;
    if (my_rank == 0)
    {
        VecCreateSeq(MPI_COMM_SELF, allstates_needed, &ws.rhs);
        VecCreateSeq(MPI_COMM_SELF, allstates_needed, &ws.x);
        MatCreateSeqDense(MPI_COMM_SELF, num_total_obs, allstates_needed, NULL, &ws.HM);
        MatCreateSeqDense(MPI_COMM_SELF, allstates_needed, allstates_needed, NULL, &ws.HTH);
        MatCreateSeqDense(MPI_COMM_SELF, allstates_needed, num_total_obs, NULL, &ws.HMTR);
        HM_col_indices = (int*)malloc(allstates_needed * sizeof(int));
        for (i = 0; i < allstates_needed; i++)
            HM_col_indices[i] = i;
        KSPCreate(MPI_COMM_SELF, &ws.ksp);
        KSPSetOperators(ws.ksp, ws.HTH, ws.HTH);
        //KSPSetTolerances(ws.ksp, 1.e-12, PETSC_DEFAULT, PETSC_DEFAULT, PETSC_DEFAULT);
        KSPSetFromOptions(ws.ksp);	// This is used to override the solver setting from the command line
    }

    int* d_indices = (int*)malloc(num_total_obs * sizeof(int));
    for (i = 0; i < num_total_obs; i++)
        d_indices[i] = i;

    //Transfer upstreams areas to all procs
    double* inv_upareas = (double*)calloc(N, sizeof(double));
    UpstreamData* updata;
    for (i = 0; i < N; i++)
    {
        if (assignments[i] == my_rank)
            inv_upareas[i] = 1.0 / (sys[i].params[globals->area_idx] * 1e3);
    }

    MPI_Allreduce(MPI_IN_PLACE, inv_upareas, N, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    //Links needed for fitting
    bool *links_needed = (bool*)calloc(N, sizeof(bool));
    for (i = 0; i < assim.num_obs; i++)
    {
        if (assignments[assim.obs_locs[i]] == my_rank)
        {
            Link *current = &sys[assim.obs_locs[i]];
            updata = (UpstreamData*)(current->user);
            links_needed[current->location] = true;
            //for(j=0;j<current->num_parents;j++)
            //{
            //	for(k=0;k<updata->num_upstreams[j];k++)
            //		my_links_needed[updata->upstreams[j][k]] = 1;
            //}

            for (j = 0; j < updata->num_upstreams; j++)
            {
                links_needed[updata->upstreams[j]->location] = true;
            }
        }
    }

    MPI_Allreduce(MPI_IN_PLACE, links_needed, N, MPI_C_BOOL, MPI_LOR, MPI_COMM_WORLD);

    //Build weight matrices
    //!!!! Assuming only q is changing !!!!
    unsigned int curr_idx = 0;
    if (my_rank == 0)
    {
        VecCreateSeq(MPI_COMM_SELF, allstates_needed, &ws.B);
        for (i = 0; i < N; i++)
        {
            if (links_needed[i])
                //VecSetValue(ws.B, curr_idx++, inv_upareas[i], INSERT_VALUES);
                VecSetValue(ws.B, curr_idx++, 1.0, INSERT_VALUES);
        }
        VecAssemblyBegin(ws.B);
        VecAssemblyEnd(ws.B);

        VecCreateSeq(MPI_COMM_SELF, num_total_obs, &ws.R);
        for (i = 0; i < assim.num_obs; i++)
        {
            for (j = 0; j < assim.num_steps; j++)
                //VecSetValue(ws.R, j*num_obs + i, inv_upareas[obs_locs[i]] * 10.0, INSERT_VALUES);
                VecSetValue(ws.R, j * assim.num_obs + i, 1.0, INSERT_VALUES);
        }
        VecAssemblyBegin(ws.R);
        VecAssemblyEnd(ws.R);

        if (verbose)
        {
            printf("Weighting Matrix B (diagonal)\n");
            VecView(ws.B, PETSC_VIEWER_STDOUT_SELF);
            printf("Weighting Matrix R (diagonal)\n");
            VecView(ws.R, PETSC_VIEWER_STDOUT_SELF);
        }
    }
    free(links_needed);

    ws.HM_buffer = (double*)calloc(allstates_needed, sizeof(double));
    ws.HM_col_indices = HM_col_indices;
    ws.d_indices = d_indices;
    ws.d_full = d_full;
    ws.x_start = x_start;
    ws.problem_dim = problem_dim;
    ws.assim_dim = assim_dim;
    ws.allstates = allstates;
    ws.allstates_needed = allstates_needed;
    ws.vareq_shift = vareq_shift;
    ws.inv_vareq_shift = inv_vareq_shift;
    ws.obs_time_step = assim.obs_time_step;
    ws.num_steps = assim.num_steps;
    ws.obs_locs = assim.obs_locs;
    ws.num_obs = assim.num_obs;
    ws.t_b = t_b;
    ws.x_b = x_b;

    //Print out some information
    unsigned int my_eqs = 0, total_eqs;
    for (i = 0; i < my_N; i++)	my_eqs += my_sys[i]->dim;
    MPI_Reduce(&my_eqs, &total_eqs, 1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);

    printf("[%i]: Good to go with %u links (%u eqs).\n", my_rank, my_N, my_eqs);
    if (my_rank == 0)
    {
        ASYNCH_SLEEP(1);
        printf("\nNetwork has a total of %u links and %u equations.\n\n", N, total_eqs);
        printf("Making calculations...\n");
    }

    MPI_Barrier(MPI_COMM_WORLD);

    //// Backup the solution
    //k = 0;
    //for (i = 0; i < N; i++)
    //    if (backup[i].dim > 0)
    //        v_copy(asynch->sys[i].list->tail->y_approx, backup[i]);

    //double simulation_time_with_data = asynch->forcings[forecast_idx]->file_time * forecaster->num_rainsteps;
    //double simulation_time_with_data = (assim.num_steps - 1) * assim.obs_time_step;
    //unsigned int simulation_time_with_data_secs = (int)(simulation_time_with_data + 1e-3) * 60;

    //Setup temp files
    //Set_Output_User_forecastparams(asynch, forecast_time_unix, simulation_time_with_data_secs);	//!!!! Should this be done here at all? !!!!
    //Set_Output_PeakflowUser_Offset(asynch,forecast_time_unix);
    //Asynch_Set_Total_Simulation_Time(asynch, forecast_window + simulation_time_with_data);
    //Asynch_Prepare_Temp_Files(asynch);

    //if (my_rank == 0)
    //{
    //    time_t now = time(NULL);
    //    struct tm* now_info = localtime(&now);
    //    printf("\n\nPass %u\n", k);
    //    printf("Current time is %s", asctime(now_info));
    //}

    //Clear buffers
    //Flush_TransData(asynch->my_data);

    //Make some initializations
    //first_file = last_file;
    //last_file = last_file + (unsigned int) asynch->forcings[forecast_idx]->file_time * 60 * num_rainsteps;
    //forecast_time_unix += change_time;
    //unsigned int forecast_idx = 0;
    //unsigned int forecast_time_unix = (unsigned int) asynch->forcings[forecast_idx]->first_file;
    //unsigned int background_time_unix = forecast_time_unix - (assim.num_steps - 1) * (unsigned int)(assim.obs_time_step * 60.0 + 1.0e-3);
    //nextforcingtime = forecast_time_unix - 60 * (unsigned int)asynch->forcings[forecast_idx]->file_time;	//This is the actual timestamp of the last needed forcing data. This will be downloaded (unlike last_file)


    unsigned int forcing_idx_rain = 0;
    unsigned int forcing_idx_tep = 1;

    unsigned int begin_assim_window = asynch->forcings[forcing_idx_rain].first_file;
    unsigned int end_assim_window = asynch->forcings[forcing_idx_rain].first_file + (unsigned int)(assim.num_steps * assim.obs_time_step * 60.0);


    //Reset each link
    //Asynch_Set_System_State(asynch, 0.0, backup);
    //Set_Output_User_forecastparams(asynch, forecast_time_unix, simulation_time_with_data_secs);
    //Set_Output_PeakflowUser_Offset(asynch,forecast_time_unix);
    //Asynch_Write_Current_Step(asynch);
    Asynch_Set_Forcing_State(asynch, forcing_idx_rain, 0.0, begin_assim_window, end_assim_window);

    for (i = 0; i < asynch->globals->num_forcings; i++)	//Set any other database forcings to begin at first_file
    {
        if (asynch->forcings[i].flag == 3)
            Asynch_Set_Forcing_State(asynch, i, 0.0, begin_assim_window, end_assim_window);
    }

    //!!!! Skipped! !!!!
            //Check if a vacuum should be done
            //This will happen at hr1
    //		if(my_rank == 0)	PerformTableMaintainance(asynch->db_connections[ASYNCH_DB_LOC_HYDRO_OUTPUT],asynch->globals,forecaster,&vac,hr1,num_tables,"archive_hydroforecast");

            //Make sure all buffer flushing is done
    MPI_Barrier(MPI_COMM_WORLD);

    ////Dump data for debugging and recovery
    //if(k % 96 == 0)
    //{
    //	sprintf(filename,"%s%u.rec",dump_filename,forecast_time_unix);
    //	Asynch_Set_Snapshot_Output_Name(asynch,filename);
    //	Asynch_Take_System_Snapshot(asynch,NULL);
    //}

    double start = MPI_Wtime();





    //Start the analysis




    //time_t q_start, q_stop;
    //int *assignments = asynch->assignments;
    //short int *getting = asynch->getting;
    //unsigned int **id_to_loc = asynch->id_to_loc, N = asynch->N, num_obs = assim.num_obs;
    //Link **sys = asynch->sys;
    //UnivVars* globals = asynch->globals;
    //unsigned int i, j, l;
    //unsigned int steps_to_use = assim.steps_to_use;
    //unsigned int assim_window_unix = assim.num_steps * (int)(assim.obs_time_step + 1e-3);
    //double t_b = 0.0, inc = assim.inc;
    //double forecast_window = forecaster->forecast_window;
    //double assim_window = assim.num_steps * assim.obs_time_step;
    //double *d_full = ws->d_full, *x_start = ws->x_start, *x_b = ws->x_b;
    //unsigned int allstates = ws->allstates;
    unsigned int max_least_squares_iters = assim.max_least_squares_iters;
    double *analysis = (double*)calloc(allstates, sizeof(double));	//!!!! Should be removed !!!!
    //model* custom_model = asynch->custom_model;
    //double q[steps_to_use*num_obs];
    double *q = (double*)calloc(num_total_obs, sizeof(double));


    //Set the forecast window
    //Asynch_Set_Total_Simulation_Duration(asynch, assim.num_steps * assim.obs_time_step);	//!!!! Is this needed? !!!!

    //Get the observations
    {
        MPI_Barrier(MPI_COMM_WORLD);
        if (my_rank == 0)
            printf("Downloading observations...\n");

        double start = MPI_Wtime();

        while (GetObservationsData(&assim, id_to_loc, N, begin_assim_window, d_full) == -1)
        {
            if (my_rank == 0)	printf("Error downloading observations. Retrying...\n");
            ASYNCH_SLEEP(5);
            MPI_Barrier(MPI_COMM_WORLD);
        }

        MPI_Barrier(MPI_COMM_WORLD);

        double stop = MPI_Wtime();

        if (my_rank == 0)
            printf("Time to get new discharges: %.0f\n", stop - start);
    }

    if (verbose && my_rank == 0)
    {
        printf("d_full\n");
        Print_VECTOR(d_full, num_total_obs);
        printf("\n");
    }

    ////Scale by upstream area
    //printf("Scaling init discharges by upstream area...\n");
    //AdjustDischarges(asynch, obs_locs, d_full, num_obs, assim_dim, x_b);

    //Set the initial guess from background
    memcpy(x_start, x_b, allstates * sizeof(double));
    //Copy in states that won't be affected by the optimization solver
    memcpy(analysis, x_b, allstates * sizeof(double));

    if (verbose && my_rank == 0)
    {
        //printf("Going in, minimizer...\n");
        //Print_VECTOR(minimizer,allstates_needed);
        //printf("Going in, x_start...\n");
        //Print_VECTOR(x_start, allstates);

        /*
        double* tempy;
        printf("Guess:\n");
        VecGetArray(Guess,&tempy);
        for(i=0;i<allstates_needed;i++)
        printf("%.15e ",tempy[i]);
        printf("\n");
        VecRestoreArray(Guess,&tempy);
        */
    }

    //Calculate the analysis
    bool try_again = false;
    do
    {
        int iterations = 0;
        double error, prev_error = -1.0;
        for (j = 0; j < max_least_squares_iters; j++)
            //while(diff > 1e-2)
        {
            iterations++;
            SolveSysLS(asynch, &ws, q);
            error = ComputeDiff(d_full, q, num_total_obs);
            if (prev_error >= 0.0)
            {
                double diff = prev_error - error;
                if (error > prev_error)
                {
                    if (my_rank == 0)
                    {
                        printf("!!!! LS error got worse. Breaking... !!!!\n");
                        printf("Errors are %f and %f\n", error, prev_error);
                    }

                    //Go back to previous solution
                    for (i = 0; i < allstates; i++)
                        x_start[i] = analysis[i];

                    break;
                }
                if (my_rank == 0)	printf("Difference is %f (%f vs %f)\n", diff, error, prev_error);
            }

            prev_error = error;
            for (i = 0; i < N; i++)
            {
                //TODO Add test for negative discharge
                analysis[i] = x_start[i] > 1.e-14 ? x_start[i] : 1.e-14;
                analysis[i + 1] = x_start[i + 1] > 0. ? x_start[i + 1] : 0.;
                analysis[i + 2] = x_start[i + 2] > 0. ? x_start[i + 2] : 0.;
                analysis[i + 3] = x_start[i + 3] > 0. ? x_start[i + 3] : 0.;
            }
        }
        if (my_rank == 0)
            printf("Total iterations = %i\n", iterations);

        //if(error > 30.0 && !try_again)	//Check if the numerical scheme is having convergence issues
        //if(error > 10.0 && !try_again)
        if (!try_again)
        {
            //try_again = 1;
            try_again = ReduceBadDischargeValues(sys, assignments, N, d_full, q, assim.num_steps, assim.obs_locs, assim.num_obs, x_start, assim_dim, 1.0);	//!!!! Not sure what to use for the limit... !!!!
        }
        else
            try_again = false;
    } while (try_again);


    //Copy x_start to analysis  !!!! This shouldn't happen. Try a pointer dance. Or maybe something better... !!!!
    //for(i=0;i<allstates;i++)	analysis[i] = x_start[i];

    if (verbose && my_rank == 0)
    {
        //if(k == 10)
        {
            printf("x_b\n");
            Print_VECTOR(x_b, allstates);
            printf("\n");

            printf("analysis [%d - %d]\n", begin_assim_window, end_assim_window);
            Print_VECTOR(analysis, allstates);
        }
    }

    free(analysis);
    free(q);
















    double stop = MPI_Wtime();
    print_out("\nTime for calculations: %f. All done!\n", stop - start);

    //Prepare snapshots
    for (i = 0; i < N; i++)
    {
        Link *current = &asynch->sys[i];
        if (current->my != NULL)
        {
            double *y = current->my->list.tail->y_approx;
            for (j = 0; j < problem_dim; j++)
                y[j] = x_start[i * problem_dim + j];

            // Idem CheckConsistency_Nonzero_4States
            if (y[0] < 1e-14)	y[0] = 1e-14;
            if (y[1] < 1e-20)	y[1] = 0.0;
            if (y[2] < 1e-20)	y[2] = 0.0;
            if (y[3] < 1e-20)	y[3] = 0.0;
        }
    }

    //Make a snaphsot
    print_out("Making snapshot\n");
    Asynch_Take_System_Snapshot(asynch, NULL);

    //Clean up
    print_out("Cleaning up\n");
    free(d);
    free(d_full);
    free(x_start);
    free(vareq_shift);
    free(inv_vareq_shift);

    if (my_rank == 0)
    {
        MatDestroy(&ws.HM);
        MatDestroy(&ws.HTH);
        VecDestroy(&ws.rhs);
        VecDestroy(&ws.x);
        VecDestroy(&ws.R);
        VecDestroy(&ws.B);
        MatDestroy(&ws.HMTR);
        KSPDestroy(&ws.ksp);
    }
    free(HM_col_indices);
    free(d_indices);

    free(inv_upareas);
    FreeAssimData(&assim);
    //Free_ForecastData(&forecaster);
    //Free_Output_PeakflowUser_Offset(asynch);
    //Free_Output_User_forecastparams(asynch);

    //Petsc clean up
    PetscFinalize();

    //Asynch clean up
    FreeUpstreamLinks(asynch);
    Asynch_Delete_Temporary_Files(asynch);
    Asynch_Free(asynch);

    return res;
}
