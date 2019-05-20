use strict;
use warnings;

use Excel::Writer::XLSX;

use Data::Dumper;

#
# Read and parse STDIN
#

# Drop warnings and text until leading '---BEGIN GREEN---'
my $found_start = 0;
my $line;
while ($line = <STDIN>) {
    chomp $line;
    if ($line eq "---BEGIN GREEN---") { $found_start = 1; last;}
}
die 'No green found!' unless $found_start;

# Read counters
my $solids_rk=0;
my $fluids_rk=1;
my $tbounds_rk=2;
my $hbounds_rk=3;
my $fbounds_rk=4;
my $ok_rk=5;
my $failed_rk=6;
$line = <STDIN>;
chomp $line;
die "Unexpected content found ($line)!" unless $line eq "# #solids #fluids #t_boundaries #h_boundaries #f_boundaries #ok #failures";
$line = <STDIN>;
chomp $line;
my @counts = $line =~ /(\d+)/g;

# Need at least 1 successful sample to proceed!
die "No successful samples in this green ($counts[$failed_rk] ) failed samples\n" unless $counts[$ok_rk];

my $last_id = -1;
my @seen_id_types;

# Read Solids
my @solids;
if ($counts[$solids_rk] > 0) {
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# Solids");
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# ID Name lambda rho cp power");
    for (my $s = 0; $s < $counts[$solids_rk] ; $s++) {
        $line = <STDIN>;
        chomp $line;
        my @tmp = split("\t", $line);
        die "Wrong number of values!" unless (scalar(@tmp) == 6);
        die "Wrong ID" unless ($tmp[0] >= 0) && (! defined $seen_id_types[$tmp[0]]);
        $seen_id_types[$tmp[0]] = 'S';
        my %new_elt = (
            ID=>$tmp[0],
            TEMP=>-1, # Imposed temperature for solids not yet in Stardis-app
            NAME=>$tmp[1],
            LAMBDA=>$tmp[2],
            RHO=>$tmp[3],
            CP=>$tmp[4],
            POWER=>$tmp[5]
            );
        push @solids, \%new_elt;
    }
}

# Read Fluids
my @fluids;
if ($counts[$fluids_rk] > 0) {
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# Fluids");
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# ID Name rho cp");
    for (my $f = 0; $f < $counts[$fluids_rk] ; $f++) {
        $line = <STDIN>;
        chomp $line;
        my @tmp = split("\t", $line);
        die "Wrong number of values!" unless (scalar(@tmp) == 4);
        die "Wrong ID" unless ($tmp[0] >= 0) && (! defined $seen_id_types[$tmp[0]]);
        $seen_id_types[$tmp[0]] = 'F';
        my %new_elt = (
            ID=>$tmp[0],
            TEMP=>-1, # Imposed temperature for fluids not yet in Stardis-app
            NAME=>$tmp[1],
            RHO=>$tmp[2],
            CP=>$tmp[3],
            POWER=>0 # Volumic Power for fluids not yet in Stardis-app
        );
        push @fluids, \%new_elt;
    }
}

# Read T Boundaries
my @t_boundaries;
if ($counts[$tbounds_rk] > 0) {
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# T Boundaries");
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# ID Name temperature");
    for (my $b = 0; $b < $counts[$tbounds_rk] ; $b++) {
        $line = <STDIN>;
        chomp $line;
        my @tmp = split("\t", $line);
        die "Wrong number of values!" unless (scalar(@tmp) == 3);
        die "Wrong ID" unless ($tmp[0] >= 0) && (! defined $seen_id_types[$tmp[0]]);
        $seen_id_types[$tmp[0]] = 'T';
        my %new_elt = (
            ID=>$tmp[0],
            NAME=>$tmp[1],
            TEMP=>$tmp[2]
        );
        push @t_boundaries, \%new_elt;
    }
}

# Read H Boundaries
my @h_boundaries;
if ($counts[$hbounds_rk] > 0) {
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# H Boundaries");
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# ID Name emissivity specular_fraction hc hc_max T_env");
    for (my $b = 0; $b < $counts[$hbounds_rk] ; $b++) {
        $line = <STDIN>;
        chomp $line;
        my @tmp = split("\t", $line);
        die "Wrong number of values!" unless (scalar(@tmp) == 7);
        die "Wrong ID" unless ($tmp[0] >= 0) &&  (! defined $seen_id_types[$tmp[0]]);
        $seen_id_types[$tmp[0]] = 'H';
        my %new_elt = (
            ID=>$tmp[0],
            NAME=>$tmp[1],
            EMISSIVITY=>$tmp[2],
            SPEC_FRACTION=>$tmp[3],
            HC=>$tmp[4],
            HC_MAX=>$tmp[5],
            T_ENV=>$tmp[6]
        );
        push @h_boundaries, \%new_elt;
    }
}

# Read F Boundaries
my @f_boundaries;
if ($counts[$fbounds_rk] > 0) {
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# F Boundaries");
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# ID Name temperature");
    for (my $b = 0; $b < $counts[$fbounds_rk] ; $b++) {
        $line = <STDIN>;
        chomp $line;
        my @tmp = split("\t", $line);
        die "Wrong number of values!" unless (scalar(@tmp) == 4);
        die "Wrong ID" unless ($tmp[0] >= 0) && (! defined $seen_id_types[$tmp[0]]);
        $seen_id_types[$tmp[0]] = 'X';
        my %new_elt = (
            ID=>$tmp[0],
            NAME=>$tmp[1],
            HC=>$tmp[2],
            HC_MAX=>$tmp[3]
            );
        push @f_boundaries, \%new_elt;
    }
}

# Read Radiative Temperatures
my $radiative_temp;
my $linear_temp;
my $rad_temp_id;
{
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# Radiative Temperatures");
    $line = <STDIN>;
    chomp $line;
    die "Unexpected content found ($line)!" unless ($line eq "# ID Rad_Temp Lin_Temp");
    $line = <STDIN>;
    chomp $line;
    my @tmp = split("\t", $line);
    die "Wrong number of values!" unless (scalar(@tmp) == 3);
    die "Wrong ID" unless ($tmp[0] >= 0) && (! defined $seen_id_types[$tmp[0]]);
    $seen_id_types[$tmp[0]] = 'R';
    die "Wrong Temperature!" unless ($tmp[1] >= 0) && ($tmp[2] >= 0);
    $radiative_temp = $tmp[1];
    $linear_temp = $tmp[2];
    $rad_temp_id = $tmp[0];
}

# Read Samples headers
$line = <STDIN>;
chomp $line;
die "Unexpected content found ($line)!" unless ($line eq "# Samples");
$line = <STDIN>;
chomp $line;
die "Unexpected content found ($line)!" unless ($line eq "# end #power_terms #flux_terms power_term_1 ... power_term_n flux_term_1 ... flux_term_n");
$line = <STDIN>;
chomp $line;
die "Unexpected content found ($line)!" unless ($line eq "# end = end_type end_id; end_type = T | H | X | R | F | S");
$line = <STDIN>;
chomp $line;
die "Unexpected content found ($line)!" unless ($line eq "# power_term_i = power_type_i power_id_i factor_i");
$line = <STDIN>;
chomp $line;
die "Unexpected content found ($line)!" unless ($line eq "# flux_term_i = flux_id_i factor_i");

# read samples
my @samples;
for(my $s=0;  $s <  $counts[$ok_rk]; $s++) {
    $line = <STDIN>;
    die "Unexpected end of data ($s samples read)" unless $line;
    chomp $line;
    my @tmp = split("\t", $line);

    # Check read data
    die "Wrong number of values!" unless (scalar(@tmp) >= 4);
    my $pw_count = $tmp[2];
    my $fx_count = $tmp[3];
    die "Wrong power_terms count!" unless ($pw_count >=0);
    die "Wrong flux_terms count!" unless ($fx_count >= 0);
    die "Wrong number of terms!" unless (scalar(@tmp) == 4 + $pw_count * 3 + $fx_count * 2);

    my @pw_types;
    my @pw_ids;
    my @pw_factors;
    my @fx_ids;
    my @fx_factors;
    for(my $n = 0; $n < $pw_count; $n++) {
        my $ty = $tmp[4+3*$n] ;
        my $id = $tmp[5+3*$n];
        my $fc =$tmp[6+3*$n];
        die "Wrong ID" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq $ty);
        die "Wrong power_term factor!" unless ($fc > 0);
        push @pw_types, $ty;
        push @pw_ids, $id;
        push @pw_factors, $fc;
    }
    for(my $n = 0; $n < $fx_count; $n++) {
        my $id = $tmp[4+$pw_count*3+2*$n];
        my $fc =$tmp[5+$pw_count*3+2*$n];
        die "Wrong ID" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq 'X');
        die "Wrong power_term factor!" unless ($fc > 0);
        push @fx_ids, $id;
        push @fx_factors, $fc;
    }
    my %new_elt = (
        END_TYPE=>$tmp[0],
        END_ID=>$tmp[1],
        PW_COUNT=>$tmp[2],
        FX_COUNT=>$tmp[3],
        PW_TYPES=>\@pw_types,
        PW_IDS=>\@pw_ids,
        PW_FACTORS=>\@pw_factors,
        FX_IDS=>\@fx_ids,
        FX_FACTORS=>\@fx_factors
        );
    push @samples, \%new_elt;
}

# Check end of data
$line = <STDIN>;
chomp $line;
die "Unexpected content in file" unless ($line eq "---END GREEN---");

#
# Export data into an xlsx file
#

binmode( STDOUT );
my $workbook = Excel::Writer::XLSX->new( \*STDOUT );
$workbook->set_properties(
    title    => 'This is an example spreadsheet',
    author   => 'Méso-Star',
    comments => 'Created with Perl, Excel::Writer::XLSX and Stardis-app Post-Process',
    );

my $title = $workbook->add_format();
$title->set_locked(1);
$title->set_bold();
$title->set_top(5);
$title->set_bottom(2);
my $locked = $workbook->add_format();
$locked->set_locked(1);
$locked->set_bg_color('#F2F2F2'); #light gray
my $unlocked = $workbook->add_format();
$unlocked->set_locked(0);

my $model_current_line = 0;
my @id_end_cells;
my @id_pw_cells;

# One sheet for all but samples
my $model = $workbook->add_worksheet('Model');
$model->protect(); # Cannot edit cells unless unlocked
$model->set_column(0, 5, 20); # Column 0 to 5: width = 20

# Create table for solids
if($counts[$solids_rk] > 0) {
   my @solid_colnames = ('Solid Name', 'Imposed Temperature', 'Lambda', 'Rho', 'Cp', 'Volumic Power');
   $model->write($model_current_line, 0, \@solid_colnames, $title);
   $model_current_line++;
   for(my $s = 0; $s < $counts[$solids_rk] ; $s++) {
       my $solid = $solids[$s];
       my $temp = $solid->{TEMP};
       my $pw = $solid->{POWER};
       $model->write($model_current_line, 0, $solid->{NAME}, $locked);
       $model->write_number($model_current_line, 1, $temp, ($temp >= 0) ? $unlocked : $locked);
       $model->write_number($model_current_line, 2, $solid->{LAMBDA}, $locked);
       $model->write_number($model_current_line, 3, $solid->{RHO}, $locked);
       $model->write_number($model_current_line, 4, $solid->{CP}, $locked);
       $model->write_number($model_current_line, 5, $pw, ($pw > 0) ? $unlocked : $locked);
       $model_current_line++;
       my $id = $solid->{ID};
       die "Inconsistency!" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq 'S');
       $id_end_cells[$id] = ($temp >= 0) ? "B$model_current_line" : 'Error';
       $id_pw_cells[$id] = "F$model_current_line";
   }
   $model_current_line++;
}

# Create table for fluids
if($counts[$fluids_rk] > 0) {
   my @fluid_colnames = ('Fluid Name', 'Imposed Temperature', 'Rho', 'Cp', 'Volumic Power');
   $model->write($model_current_line, 0, \@fluid_colnames, $title);
   $model_current_line++;
   for(my $f = 0; $f < $counts[$fluids_rk] ; $f++) {
       my $fluid = $fluids[$f];
       my $temp = $fluid->{TEMP};
       my $pw =  $fluid->{POWER};
       $model->write($model_current_line, 0, $fluid->{NAME}, $locked);
       $model->write_number($model_current_line, 1, $temp, ($temp >= 0) ? $unlocked : $locked);
       $model->write_number($model_current_line, 2, $fluid->{RHO}, $locked);
       $model->write_number($model_current_line, 3, $fluid->{CP}, $locked);
       $model->write_number($model_current_line, 4, $pw, ($pw > 0) ? $unlocked : $locked);
       $model_current_line++;
       my $id = $fluid->{ID};
       die "Inconsistency!" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq 'F');
       $id_end_cells[$id] = ($temp >= 0) ? "B$model_current_line" : 'Error';
       $id_pw_cells[$id] = "E$model_current_line";
   }
   $model_current_line++;
}

# Create table for T boundaries
if($counts[$tbounds_rk] > 0) {
   my @tbound_colnames = ('T Boundary Name', 'Temperature');
   $model->write($model_current_line, 0, \@tbound_colnames, $title);
   $model_current_line++;
   for(my $b = 0; $b < $counts[$tbounds_rk] ; $b++) {
       my $tbound = $t_boundaries[$b];
       my $temp = $tbound->{TEMP};
       $model->write($model_current_line, 0, $tbound->{NAME}, $locked);
       $model->write_number($model_current_line, 1, $temp, ($temp >= 0) ? $unlocked : $locked);
       $model_current_line++;
       my $id = $tbound->{ID};
       die "Inconsistency!" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq 'T');
       $id_end_cells[$id]= "B$model_current_line";
       $id_pw_cells[$id] = "Error";
   }
   $model_current_line++;
}

# Create table for H boundaries
if($counts[$hbounds_rk] > 0) {
   my @hbound_colnames = ('H Boundary Name', 'Emissivity', 'Specular Fraction', 'Hc', 'Hc_max', 'T_env');
   $model->write($model_current_line, 0, \@hbound_colnames, $title);
   $model_current_line++;
   for(my $b = 0; $b < $counts[$hbounds_rk] ; $b++) {
       my $hbound = $h_boundaries[$b];
       $model->write($model_current_line, 0, $hbound->{NAME}, $locked);
       $model->write_number($model_current_line, 1, $hbound->{EMISSIVITY}, $locked);
       $model->write_number($model_current_line, 2, $hbound->{SPEC_FRACTION}, $locked);
       $model->write_number($model_current_line, 3, $hbound->{HC}, $locked);
       $model->write_number($model_current_line, 4, $hbound->{HC_MAX}, $locked);
       $model->write_number($model_current_line, 5, $hbound->{T_ENV}, $unlocked);
       $model_current_line++;
       my $id = $hbound->{ID};
       die "Inconsistency!" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq 'H');
       $id_end_cells[$id]= "F$model_current_line";
       $id_pw_cells[$id] = "Error"; # No volumic power at boundaries!
   }
   $model_current_line++;
}

# Create table for F boundaries
if($counts[$fbounds_rk] > 0) {
   my @fbound_colnames = ('F Boundary Name', 'hc', 'hc_max');
   $model->write($model_current_line, 0, \@fbound_colnames, $title);
   $model_current_line++;
   for(my $b = 0; $b < $counts[$fbounds_rk] ; $b++) {
       my $fbound = $f_boundaries[$b];
       $model->write($model_current_line, 0, $fbound->{NAME}, $locked);
       $model->write_number($model_current_line, 1, $fbound->{HC}, $locked);
       $model->write_number($model_current_line, 2, $fbound->{HC_MAX}, $locked);
       $model_current_line++;
       my $id = $fbound->{ID};
       die "Inconsistency!" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq 'X');
       $id_end_cells[$id]= "Error"; # Cannot end at F boundaries
       $id_pw_cells[$id] = "Error"; # No volumic power at boundaries!
   }
   $model_current_line++;
}

# Create table for Radiative Temperatures
{
   my @radiative_colnames = ('Radiative Temperature', 'Linearisation Temperature');
   $model->write($model_current_line, 0, \@radiative_colnames, $title);
   $model_current_line++;
   $model->write_number($model_current_line, 0, $radiative_temp, $unlocked);
   $model->write_number($model_current_line, 1, $linear_temp, $locked);
   $model_current_line++;
   $id_end_cells[$rad_temp_id]= "A$model_current_line";
   $id_pw_cells[$rad_temp_id] = "Error"; # No volumic power at infinity!
   $model_current_line++;
}

# One sheet for samples
my $green = $workbook->add_worksheet('Samples');
$green->protect();

my $samples_current_line = 0;
if($counts[$ok_rk] > 0) {
   my @samples_colnames = ('T(sample)');
   $green->write($samples_current_line, 0, \@samples_colnames, $title);
   $samples_current_line++;
   for(my $s = 0; $s < $counts[$ok_rk] ; $s++) {
       my $sample = $samples[$s];
       my $end_type = $sample->{END_TYPE};
       my $end_id = $sample->{END_ID};
       my $pw_count = $sample->{PW_COUNT};
       my $pw_types = $sample->{PW_TYPES};
       my $pw_ids =  $sample->{PW_IDS};
       my $pw_factors = $sample->{PW_FACTORS};
       my $fx_count = $sample->{FX_COUNT};
       my $fx_ids = $sample->{FX_IDS};
       my $fx_factors = $sample->{FX_FACTORS};
              
       # create a cell with formula:
       #     end_temp + pw_term_1 + ... + pw_term_n + fx_term_1 + ... + fx_term_n
       unless (($end_id >= 0) && ($end_id < scalar(@seen_id_types)) && (defined $seen_id_types[$end_id]) && ($seen_id_types[$end_id] eq $end_type)) {
           print STDERR "Sample: ", Dumper($sample);
           print STDERR "END ID: $end_id\n";
           print STDERR "END TYPE: $seen_id_types[$end_id] VS $end_type\n";
           print STDERR "END TYPES: @seen_id_types\n";
           die "Invalid ID!" ;
       }
       unless ($id_end_cells[$end_id] ne 'Error') {
           print STDERR "Sample: ", Dumper($sample);
           print STDERR "END ID: $end_id\n";
           print STDERR "END END CELLS: @id_end_cells\n";
           die "Inconsistency!"
       }
       my $formula = "=Model!$id_end_cells[$end_id]";
       for(my $n = 0; $n < $pw_count; $n++) {
           my $ty = @$pw_types[$n];
           my $id = @$pw_ids[$n];
           my $fc = @$pw_factors[$n];
           die "Invalid ID ($id)!" unless ($id >= 0) && (defined $seen_id_types[$id]) && ($seen_id_types[$id] eq $ty);
           die "Inconsistency!" unless ($id_pw_cells[$id] ne 'Error');
           $formula = $formula."+Model!$id_pw_cells[$id]*$fc";
       }
       $green->write_formula($samples_current_line, 0, $formula, $locked);
       $samples_current_line++;
   }
}

# The MC Estimator and STDERR table
my @result_colnames = ('Estimate', 'Sigma');
$model->write($model_current_line, 0, \@result_colnames, $title);
$model_current_line++;
$model->write_formula($model_current_line, 0, "AVERAGE(Samples!A2:A$samples_current_line)", $locked);
$model->write_formula($model_current_line, 1, "STDEV(Samples!A2:A$samples_current_line)/SQRT($samples_current_line-1)", $locked);
$model_current_line++;

$workbook->close();


#print "Read $line\n";
#print Dumper(@samples);
#print $fluids[0]->{ID};
