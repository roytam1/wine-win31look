#
# Copyright 2002 Patrik Stridvall
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

package c_type;

use strict;

use output qw($output);

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {};
    bless ($self, $class);

    return $self;
}

########################################################################
# set_find_align_callback
#
sub set_find_align_callback {
    my $self = shift;

    my $find_align = \${$self->{FIND_ALIGN}};

    $$find_align = shift;
}

########################################################################
# set_find_kind_callback
#
sub set_find_kind_callback {
    my $self = shift;

    my $find_kind = \${$self->{FIND_KIND}};

    $$find_kind = shift;
}

########################################################################
# set_find_size_callback
#
sub set_find_size_callback {
    my $self = shift;

    my $find_size = \${$self->{FIND_SIZE}};

    $$find_size = shift;
}

sub kind {
    my $self = shift;
    my $kind = \${$self->{KIND}};
    my $dirty = \${$self->{DIRTY}};

    local $_ = shift;

    if(defined($_)) { $$kind = $_; $$dirty = 1; }

    if (!defined($$kind)) {
	$self->_refresh();
    }

    return $$kind;
}

sub _name {
    my $self = shift;
    my $_name = \${$self->{_NAME}};
    my $dirty = \${$self->{DIRTY}};

    local $_ = shift;

    if(defined($_)) { $$_name = $_; $$dirty = 1; }

    return $$_name;
}

sub name {
    my $self = shift;
    my $name = \${$self->{NAME}};
    my $dirty = \${$self->{DIRTY}};

    local $_ = shift;

    if(defined($_)) { $$name = $_; $$dirty = 1; }

    if($$name) {
	return $$name;
    } else {
	my $kind = \${$self->{KIND}};
	my $_name = \${$self->{_NAME}};

	return "$$kind $$_name";
    }
}

sub pack {
    my $self = shift;
    my $pack = \${$self->{PACK}};
    my $dirty = \${$self->{DIRTY}};
    
    local $_ = shift;

    if(defined($_)) { $$pack = $_; $$dirty = 1; }

    return $$pack;
}

sub align {
    my $self = shift;

    my $align = \${$self->{ALIGN}};

    $self->_refresh();

    return $$align;
}

sub fields {
    my $self = shift;

    my $count = $self->field_count;

    my @fields = ();
    for (my $n = 0; $n < $count; $n++) {
	my $field = 'c_type_field'->new($self, $n);
	push @fields, $field;
    }
    return @fields;
}

sub field_base_sizes {
    my $self = shift;
    my $field_base_sizes = \${$self->{FIELD_BASE_SIZES}};

    $self->_refresh();

    return $$field_base_sizes;
}

sub field_aligns {
    my $self = shift;
    my $field_aligns = \${$self->{FIELD_ALIGNS}};

    $self->_refresh();

    return $$field_aligns;
}

sub field_count {
    my $self = shift;
    my $field_type_names = \${$self->{FIELD_TYPE_NAMES}};

    my @field_type_names = @{$$field_type_names}; 
    my $count = scalar(@field_type_names);

    return $count;
}

sub field_names {
    my $self = shift;
    my $field_names = \${$self->{FIELD_NAMES}};
    my $dirty = \${$self->{DIRTY}};

    local $_ = shift;

    if(defined($_)) { $$field_names = $_; $$dirty = 1; }

    return $$field_names;
}

sub field_offsets {
    my $self = shift;
    my $field_offsets = \${$self->{FIELD_OFFSETS}};

    $self->_refresh();

    return $$field_offsets;
}

sub field_sizes {
    my $self = shift;
    my $field_sizes = \${$self->{FIELD_SIZES}};

    $self->_refresh();

    return $$field_sizes;
}

sub field_type_names {
    my $self = shift;
    my $field_type_names = \${$self->{FIELD_TYPE_NAMES}};
    my $dirty = \${$self->{DIRTY}};

    local $_ = shift;

    if(defined($_)) { $$field_type_names = $_; $$dirty = 1; }

    return $$field_type_names;
}

sub size {
    my $self = shift;

    my $size = \${$self->{SIZE}};

    $self->_refresh();

    return $$size;
}

sub _refresh {
    my $self = shift;

    my $dirty = \${$self->{DIRTY}};

    return if !$$dirty;

    my $find_align = \${$self->{FIND_ALIGN}};
    my $find_kind = \${$self->{FIND_KIND}};
    my $find_size = \${$self->{FIND_SIZE}};

    my $align = \${$self->{ALIGN}};
    my $kind = \${$self->{KIND}};
    my $size = \${$self->{SIZE}};
    my $field_aligns = \${$self->{FIELD_ALIGNS}};
    my $field_base_sizes = \${$self->{FIELD_BASE_SIZES}};
    my $field_offsets = \${$self->{FIELD_OFFSETS}};
    my $field_sizes = \${$self->{FIELD_SIZES}};

    my $pack = $self->pack;
    $pack = 4 if !defined($pack);

    my $max_field_align = 0;

    my $offset = 0;
    my $offset_bits = 0;

    my $n = 0;
    foreach my $field ($self->fields) {
	my $type_name = $field->type_name;
	my $type_size = &$$find_size($type_name);

	my $base_type_name = $type_name;
	if ($base_type_name =~ s/^(.*?)\s*(?:\[\s*(.*?)\s*\]|:(\d+))?$/$1/) {
	    my $count = $2;
	    my $bits = $3;
	}
	my $base_size = &$$find_size($base_type_name);
	$$align = &$$find_align($base_type_name);

	if (defined($$align)) { 
	    $$align = $pack if $$align > $pack;
	    $max_field_align = $$align if $$align > $max_field_align;

	    if ($offset % $$align != 0) {
		$offset = (int($offset / $$align) + 1) * $$align;
	    }
	}

	if ($$kind !~ /^(?:struct|union)$/) {
	    $$kind = &$$find_kind($type_name) || "";
	}

	if (!defined($type_size)) {
	    $$align = undef;
	    $$size = undef;
	    return;
	} elsif ($type_size >= 0) {
	    if ($offset_bits) {
		$offset += $pack * int(($offset_bits + 8 * $pack - 1 ) / (8 * $pack));
		$offset_bits = 0;
	    }

	    $$$field_aligns[$n] = $$align;
	    $$$field_base_sizes[$n] = $base_size;
	    $$$field_offsets[$n] = $offset;
	    $$$field_sizes[$n] = $type_size;

	    $offset += $type_size;
	} else {
	    $$$field_aligns[$n] = $$align;
	    $$$field_base_sizes[$n] = $base_size;
	    $$$field_offsets[$n] = $offset;
	    $$$field_sizes[$n] = $type_size;

	    $offset_bits += -$type_size;
	}

	$n++;
    }

    $$align = $pack;
    $$align = $max_field_align if $max_field_align < $pack;

    if ($offset_bits) {
	$offset += $pack * int(($offset_bits + 8 * $pack - 1 ) / (8 * $pack));
	$offset_bits = 0;
    }

    $$size = $offset;
    if ($$kind =~ /^(?:struct|union)$/) {
	if ($$size % $$align != 0) {
	    $$size = (int($$size / $$align) + 1) * $$align;
	}
    }

    $$dirty = 0;
}

package c_type_field;

sub new {
    my $proto = shift;
    my $class = ref($proto) || $proto;
    my $self  = {};
    bless ($self, $class);

    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    $$type = shift;
    $$number = shift;

    return $self;
}

sub align {
    my $self = shift;
    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    my $field_aligns = $$type->field_aligns;

    return $$field_aligns[$$number];
}

sub base_size {
    my $self = shift;
    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    my $field_base_sizes = $$type->field_base_sizes;

    return $$field_base_sizes[$$number];
}

sub name {
    my $self = shift;
    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    my $field_names = $$type->field_names;

    return $$field_names[$$number];
}

sub offset {
    my $self = shift;
    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    my $field_offsets = $$type->field_offsets;

    return $$field_offsets[$$number];
}

sub size {
    my $self = shift;
    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    my $field_sizes = $$type->field_sizes;

    return $$field_sizes[$$number];
}

sub type_name {
    my $self = shift;
    my $type = \${$self->{TYPE}};
    my $number = \${$self->{NUMBER}};

    my $field_type_names = $$type->field_type_names;

    return $$field_type_names[$$number];
}

1;
