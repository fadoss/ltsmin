// -*- tab-width:4 ; indent-tabs-mode:nil -*-
#include <config.h>

#include <stdio.h>

#include <chunk_support.h>
#include <hre/user.h>
#include <hre/runtime.h>
#include <lts-io/user.h>
#include <lts-lib/lts.h>

#define  BUFLEN 4096

static int arg_all=0;
static int arg_table=0;
static char *arg_value="name";
static enum { FF_TXT, FF_CSV } file_format = FF_TXT;
static char *arg_sep=",";
static enum { IDX, NAME } output_value = NAME;

static si_map_entry output_values[] = {
    {"name",    NAME},
    {"idx",     IDX},
    {NULL, 0}
};

static si_map_entry file_formats[] = {
    {"txt",     FF_TXT},
    {"csv",     FF_CSV},
    {NULL, 0}
};

static void
output_popt (poptContext con, enum poptCallbackReason reason,
               const struct poptOption *opt, const char *arg, void *data)
{
    (void)con; (void)opt; (void)arg; (void)data;
    switch (reason) {
    case POPT_CALLBACK_REASON_PRE:
        break;
    case POPT_CALLBACK_REASON_POST: {
            int ov = linear_search (output_values, arg_value);
            if (ov < 0) {
                Warning (error, "unknown output value %s", arg_value);
                HREprintUsage();
                HREabort(EXIT_FAILURE);
            }
            output_value = ov;
        }
        return;
    case POPT_CALLBACK_REASON_OPTION:
        break;
    }
    Fatal (1, error, "unexpected call to output_popt");
}

static  struct poptOption options[] = {
    { NULL, 0 , POPT_ARG_CALLBACK|POPT_CBFLAG_POST|POPT_CBFLAG_SKIPOPTION  , (void*)output_popt , 0 , NULL , NULL },
    { "csv-separator" , 's' , POPT_ARG_STRING , &arg_sep , 0 , "separator in csv output" , NULL },
    { "values" , 0 , POPT_ARG_STRING|POPT_ARGFLAG_SHOW_DEFAULT , &arg_value , 0 ,
      "output indices/names", "<idx|name>"},
    { "table" , 't' , POPT_ARG_VAL , &arg_table , 1 , "output txt format as table" , NULL },
    { "list" , 'l' , POPT_ARG_VAL , &arg_table , 0 , "output txt format as list", NULL },
    { "diff" , 'd' , POPT_ARG_VAL , &arg_all , 0 , "output state differences instead of all values" , NULL },
    { "all" , 'a' , POPT_ARG_VAL , &arg_all , 1 , "output all values instead of state differences" , NULL },
    POPT_TABLEEND
};

static void
trace_get_type_str(lts_t trace, int typeno, int type_idx, size_t dst_size, char* dst) {
    if (typeno==-1) Abort("illegal type");
    if ( output_value == IDX ) {
        snprintf(dst, dst_size, "%d", type_idx);
        return;
    }
    switch(lts_type_get_format(trace->ltstype,typeno)){
        case LTStypeDirect:
        case LTStypeRange:
            snprintf(dst, dst_size, "%d", type_idx);
            break;
        case LTStypeChunk:
        case LTStypeEnum:
            {
            chunk c=VTgetChunk(trace->values[typeno],type_idx);
            chunk2string(c,dst_size,dst);
            }
            break;
    }
}

void
output_text(lts_t trace, FILE* output_file) {
    int N = lts_type_get_state_length(trace->ltstype);
    int eLbls = lts_type_get_edge_label_count(trace->ltstype);
    int sLbls = lts_type_get_state_label_count(trace->ltstype);
    uint32_t align = 0;  // maximal width of state header/state label header
    char tmp[BUFLEN];
    char tmp2[BUFLEN];

    // calculate width
    for(int j=0; j<N; ++j) {
        // state header
        char *name = lts_type_get_state_name(trace->ltstype, j);
        char *type = lts_type_get_state_type(trace->ltstype, j);
        snprintf(tmp, BUFLEN, "%s:%s[%d]", name == NULL ? "_" : name, type == NULL ? "_" : type, j);
        if (align < strlen(tmp)) align = strlen(tmp);
    }
    for(int j=0; j<sLbls; ++j) {
        char *name = lts_type_get_state_label_name(trace->ltstype, j);
        char *type = lts_type_get_state_label_type(trace->ltstype, j);
        snprintf(tmp, BUFLEN, "%s:%s[%d]", name == NULL ? "_" : name, type == NULL ? "_" : type, j);
        if (align < strlen(tmp)) align = strlen(tmp);
    }

    // output trace
    for(uint32_t i=0; i<=trace->transitions; ++i) {
        int prev_state[N];
        int state[N];
        int prev_state_lbls[sLbls];
        int state_lbls[sLbls];
        int edge_lbls[eLbls];

        fprintf(output_file, "state %d/%d\n",i,trace->transitions);
        if (i == 0) {
            // ouput initial state
            fprintf(output_file, "Initial state\n");
            for(int j=0; j<N; ++j) prev_state[j] = -1;
            for(int j=0; j<sLbls; ++j) prev_state_lbls[j] = -1;
        } else {
            // last state
            for(int j=0; j<N; ++j) prev_state[j] = state[j];
            for(int j=0; j<sLbls; ++j) prev_state_lbls[j] = state_lbls[j];
        }

        // get state
        if (N) {
            TreeUnfold(trace->state_db, i, state);
        }

        // output state
        for(int j=0; j<N; ++j) {
            if (arg_all || state[j] != prev_state[j]) {
                char *name = lts_type_get_state_name(trace->ltstype, j);
                char *type = lts_type_get_state_type(trace->ltstype, j);
                snprintf(tmp, BUFLEN, "%s:%s[%d]", name == NULL ? "_" : name, type == NULL ? "_" : type, j);

                int typeno = lts_type_get_state_typeno(trace->ltstype, j);
                trace_get_type_str(trace, typeno, state[j], BUFLEN, tmp2);

                fprintf(output_file, "\t%*s = %s\n", align, tmp, tmp2 );
            }
        }

        // output state labels
        if (trace->properties != NULL) {
            if (trace->prop_idx) {
                TreeUnfold(trace->prop_idx, trace->properties[i], state_lbls);
            } else {
                state_lbls[0]=trace->properties[i];
            }
            for(int j=0; j<sLbls; ++j) {
                if (arg_all || state_lbls[j] != prev_state_lbls[j]) {
                    char *name = lts_type_get_state_label_name(trace->ltstype, j);
                    char *type = lts_type_get_state_label_type(trace->ltstype, j);
                    snprintf(tmp, BUFLEN, "%s:%s[%d]", name == NULL ? "_" : name, type == NULL ? "_" : type, j);

                    int typeno = lts_type_get_state_label_typeno(trace->ltstype, j);
                    trace_get_type_str(trace, typeno, state_lbls[j], BUFLEN, tmp2);

                    fprintf(output_file, "\t%*s = %s\n", align, tmp, tmp2);
                }
            }
        }

        // output edge labels
        if (i<trace->transitions) {
            if (trace->label != NULL) {
                if (trace->edge_idx){
                    TreeUnfold(trace->edge_idx, trace->label[i], edge_lbls);
                } else {
                    edge_lbls[0]=trace->label[i];
                }
                for(int j=0; j<eLbls; ++j) {
                    char *name = lts_type_get_edge_label_name(trace->ltstype, j);
                    char *type = lts_type_get_edge_label_type(trace->ltstype, j);
                    snprintf(tmp, BUFLEN, "%s:%s[%d]", name == NULL ? "_" : name, type == NULL ? "_" : type, j);

                    int typeno = lts_type_get_edge_label_typeno(trace->ltstype, j);
                    trace_get_type_str(trace, typeno, edge_lbls[j], BUFLEN, tmp2);

                    fprintf(output_file, "%s%s = %s",j==0?"":", ", tmp, tmp2);
                }
                fprintf(output_file, "\n");
            } else {
                fprintf(output_file," --- no edge label ---\n");
            }
        }
    }
}

void
output_text_table(lts_t trace, FILE* output_file) {
    int N = lts_type_get_state_length(trace->ltstype);
    int eLbls = lts_type_get_edge_label_count(trace->ltstype);
    int sLbls = lts_type_get_state_label_count(trace->ltstype);
    int width_s[N];      // width of state item column
    int width_el[eLbls]; // width of edge label column
    int width_sl[sLbls]; // width of state label column
    char tmp[BUFLEN];

    // calculate width
    for(int j=0; j<N; ++j) {
        // state header
        char *name = lts_type_get_state_name(trace->ltstype, j);
        char *type = lts_type_get_state_type(trace->ltstype, j);
        snprintf(tmp, BUFLEN, "%s:%s", name == NULL ? "_" : name, type == NULL ? "_" : type);
        width_s[j] = strlen(tmp);

    }
    for(int j=0; j<sLbls; ++j) {
        char *name = lts_type_get_state_label_name(trace->ltstype, j);
        char *type = lts_type_get_state_label_type(trace->ltstype, j);
        snprintf(tmp, BUFLEN, "%s:%s", name == NULL ? "_" : name, type == NULL ? "_" : type);
        width_sl[j] = strlen(tmp);
    }
    for(int j=0; j<eLbls; ++j) {
        char *name = lts_type_get_edge_label_name(trace->ltstype, j);
        char *type = lts_type_get_edge_label_type(trace->ltstype, j);
        snprintf(tmp, BUFLEN, "%s:%s", name == NULL ? "_" : name, type == NULL ? "_" : type);
        width_el[j] = strlen(tmp);
    }
    for(uint32_t i=0; i<trace->states; ++i) {
        int state[N];
        if (N) TreeUnfold(trace->state_db, i, state);

        for(int j=0; j<N; ++j) {
            int typeno = lts_type_get_state_typeno(trace->ltstype, j);
            trace_get_type_str(trace, typeno, state[j], BUFLEN, tmp);
            int len = strlen(tmp);
            if (width_s[j] < len) width_s[j] = len;
        }
        if (trace->properties != NULL) {
            int state_lbls[sLbls];
            if (trace->prop_idx) {
                TreeUnfold(trace->prop_idx, trace->properties[i], state_lbls);
            } else {
                state_lbls[0]=trace->properties[i];
            }
            for(int j=0; j<sLbls; ++j) {
                int typeno = lts_type_get_state_label_typeno(trace->ltstype, j);
                trace_get_type_str(trace, typeno, state_lbls[j], BUFLEN, tmp);
                int len = strlen(tmp);
                if (width_sl[j] < len) width_sl[j] = len;
            }
        }
        if ((i+1) < trace->states && trace->label != NULL) {
            int edge_lbls[eLbls];
            if (trace->edge_idx){
                TreeUnfold(trace->edge_idx, trace->label[i], edge_lbls);
            } else {
                edge_lbls[0]=trace->label[i];
            }
            for(int j=0; j<eLbls; ++j) {
                int typeno = lts_type_get_edge_label_typeno(trace->ltstype, j);
                trace_get_type_str(trace, typeno, edge_lbls[j], BUFLEN, tmp);
                int len = strlen(tmp);
                if (width_el[j] < len) width_el[j] = len;
            }
        }
    }
    // print header
    fprintf(output_file, "      ");
    for(int j=0; j<N; ++j) {
        char *name = lts_type_get_state_name(trace->ltstype, j);
        char *type = lts_type_get_state_type(trace->ltstype, j);
        snprintf(tmp, BUFLEN, "%s:%s", name == NULL ? "_" : name, type == NULL ? "_" : type);

        fprintf(output_file, "%s%*s", j==0?"":" ", -width_s[j], tmp);
    }
    if (trace->properties != NULL) {
        fprintf(output_file, "   ");
        for(int j=0; j<sLbls; ++j) {
            char *name = lts_type_get_state_label_name(trace->ltstype, j);
            char *type = lts_type_get_state_label_type(trace->ltstype, j);
            snprintf(tmp, BUFLEN, "%s:%s", name == NULL ? "_" : name, type == NULL ? "_" : type);

            fprintf(output_file, "%s%*s", j==0?"":" ", -width_sl[j], tmp);
        }
    }
    if (trace->label != NULL) {
        fprintf(output_file, "   ");
        for(int j=0; j<eLbls; ++j) {
            char *name = lts_type_get_edge_label_name(trace->ltstype, j);
            char *type = lts_type_get_edge_label_type(trace->ltstype, j);
            snprintf(tmp, BUFLEN, "%s:%s", name == NULL ? "_" : name, type == NULL ? "_" : type);

            fprintf(output_file, "%s%*s", j==0?"":" ", -width_el[j], tmp);
        }
    }
    fprintf(output_file, "\n");


    // print the state / state labels / edge labels
    for(uint32_t i=0; i<trace->states; ++i) {
        int prev_state[N];
        int state[N];
        int prev_state_lbls[sLbls];
        int state_lbls[sLbls];
        int prev_edge_lbls[eLbls];
        int edge_lbls[eLbls];
        for(int j=0; j<N; ++j) prev_state[j] = (i == 0 ? -1 : state[j]);
        if (N) TreeUnfold(trace->state_db, i, state);
        fprintf(output_file, "%.3d: [",i);
        for(int j=0; j<N; ++j) {
            if (arg_all || state[j] != prev_state[j]) {
                int typeno = lts_type_get_state_typeno(trace->ltstype, j);
                trace_get_type_str(trace, typeno, state[j], BUFLEN, tmp);
            } else {
                snprintf(tmp, BUFLEN, "...");
            }

            fprintf(output_file, "%s%*s", j==0?"":" ", width_s[j], tmp);
        }
        fprintf(output_file, "]");

        // print state labels
        if (trace->properties != NULL) {
            fprintf(output_file, " [");
            for(int j=0; j<sLbls; ++j) prev_state_lbls[j] = (i == 0 ? -1 : state_lbls[j]);
            if (trace->prop_idx) {
                TreeUnfold(trace->prop_idx, trace->properties[i], state_lbls);
            } else {
                state_lbls[0]=trace->properties[i];
            }
            for(int j=0; j<sLbls; ++j) {
                if (arg_all || state_lbls[j] != prev_state_lbls[j]) {
                    int typeno = lts_type_get_state_label_typeno(trace->ltstype, j);
                    trace_get_type_str(trace, typeno, state_lbls[j], BUFLEN, tmp);
                } else {
                    snprintf(tmp, BUFLEN, "...");
                }

                fprintf(output_file, "%s%*s", j==0?"":" ", width_sl[j], tmp);
            }
            fprintf(output_file, "]");
        }

        // print edge labels
        if ((i+1)<trace->states) {
            if (trace->label != NULL) {
                fprintf(output_file, " [");
                for(int j=0; j<eLbls; ++j) prev_edge_lbls[j] = (i == 0 ? -1 : edge_lbls[j]);
                if (trace->edge_idx){
                    TreeUnfold(trace->edge_idx, trace->label[i], edge_lbls);
                } else {
                    edge_lbls[0]=trace->label[i];
                }
                for(int j=0; j<eLbls; ++j) {
                    if (arg_all || edge_lbls[j] != prev_edge_lbls[j]) {
                        int typeno = lts_type_get_edge_label_typeno(trace->ltstype, j);
                        trace_get_type_str(trace, typeno, edge_lbls[j], BUFLEN, tmp);
                    } else {
                        snprintf(tmp, BUFLEN, "...");
                    }

                    fprintf(output_file, "%s%*s", j==0?"":" ", width_el[j], tmp);
                }
                fprintf(output_file, "]");
            }
        }
        fprintf(output_file, "\n");
    }
}

void
output_csv(lts_t trace, FILE* output_file) {
    int N = lts_type_get_state_length(trace->ltstype);
    int eLbls = lts_type_get_edge_label_count(trace->ltstype);
    int sLbls = lts_type_get_state_label_count(trace->ltstype);

    // add header
    for(int j=0; j<N; ++j) {
        char *name = lts_type_get_state_name(trace->ltstype, j);
        char *type = lts_type_get_state_type(trace->ltstype, j);
        fprintf(output_file, "%s%s:%s", j==0 ? "" : arg_sep, name == NULL ? "_" : name, type == NULL ? "_" : type);
    }
    if (trace->properties != NULL) {
        for(int j=0; j<sLbls; ++j) {
            char *name = lts_type_get_state_label_name(trace->ltstype, j);
            char *type = lts_type_get_state_label_type(trace->ltstype, j);
            fprintf(output_file, "%s%s:%s", arg_sep, name == NULL ? "_" : name, type == NULL ? "_" : type);
        }
    }
    if (trace->label != NULL) {
        for(int j=0; j<eLbls; ++j) {
            char *name = lts_type_get_edge_label_name(trace->ltstype, j);
            char *type = lts_type_get_edge_label_type(trace->ltstype, j);
            fprintf(output_file, "%s%s:%s", arg_sep, name == NULL ? "_" : name, type == NULL ? "_" : type);
        }
    }
    fprintf(output_file, "\n");

    // print the state / state labels / edge labels
    for(uint32_t i=0; i<trace->states; ++i) {
        int state[N];
        char tmp[BUFLEN];
        if (N) TreeUnfold(trace->state_db, i, state);
        for(int j=0; j<N; ++j) {
            int typeno = lts_type_get_state_typeno(trace->ltstype, j);
            trace_get_type_str(trace, typeno, state[j], BUFLEN, tmp);
            fprintf(output_file, "%s%s", j==0 ? "" : arg_sep, tmp);
        }

        // print state labels
        if (trace->properties != NULL) {
            int state_lbls[sLbls];
            if (trace->prop_idx) {
                TreeUnfold(trace->prop_idx, trace->properties[i], state_lbls);
            } else {
                state_lbls[0]=trace->properties[i];
            }
            for(int j=0; j<sLbls; ++j) {
                int typeno = lts_type_get_state_label_typeno(trace->ltstype, j);
                trace_get_type_str(trace, typeno, state_lbls[j], BUFLEN, tmp);
                fprintf(output_file, "%s%s", arg_sep, tmp);
            }
        }

        // printf edge labels
        if (trace->label != NULL) {
            int edge_lbls[eLbls];
            if (trace->edge_idx){
                TreeUnfold(trace->edge_idx, trace->label[i], edge_lbls);
            } else {
                edge_lbls[0]=trace->label[i];
            }
            for(int j=0; j<eLbls; ++j) {
                if ((i+1)<trace->states) {
                    int typeno = lts_type_get_edge_label_typeno(trace->ltstype, j);
                    trace_get_type_str(trace, typeno, edge_lbls[j], BUFLEN, tmp);
                    fprintf(output_file, "%s%s", arg_sep, tmp);
                } else {
                    fprintf(output_file, "%s", arg_sep);
                }
            }
        }
        fprintf(output_file, "\n");
    }
}

int
main(int argc,char*argv[]){
    char *files[2];
    HREinitBegin(argv[0]);
    HREaddOptions(options,"Pretty print trace files\n\n"
                "Supported output file extensions are:\n"
                "  txt: Textual output\n"
                "  csv: Comma separated values\n\n"
                "Options");
    lts_lib_setup();
    HREinitStart(&argc,&argv,1,2,files,"<input> [<output>]");
    // open file (--file argument or stdout in case of NULL)
    FILE* output_file = stdout;
    if (files[1]) {
        // determine extension
        char *extension = strrchr (files[1], '.');
        if (extension == NULL) {
            Fatal(1,error,"unknown file format extension for file '%s'", files[1]);
        }
        extension++;
        int ff = linear_search (file_formats, extension);
        if (ff < 0) {
            Fatal(1,error,"unknown file format '%s'", extension);
        }
        file_format = ff;

        // open file
        Warning(info,"Writing output to '%s'",files[1]);
        output_file = fopen(files[1],"w");
        if (output_file == NULL) {
            Fatal(1,error,"Could not open file '%s'\n", files[1]);
        }
    }

    lts_t trace=lts_create();
    lts_read(files[0],trace);
    Warning(info,"length of trace is %d",trace->transitions);

    switch (file_format) {
        case FF_TXT:
            if (arg_table) {
                output_text_table(trace, output_file);
            } else {
                output_text(trace, output_file);
            }
            break;
        case FF_CSV:
            output_csv(trace, output_file);
            break;
        default:
            Fatal(1,error,"File format no(t) yet/longer supported!");
    }

    // close output file
    if (files[1]) fclose(output_file);
    HREexit(EXIT_SUCCESS);
}