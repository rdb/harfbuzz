/*
 * Copyright © 2017  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_AAT_LAYOUT_MORX_TABLE_HH
#define HB_AAT_LAYOUT_MORX_TABLE_HH

#include "hb-open-type-private.hh"
#include "hb-aat-layout-common-private.hh"

#define HB_AAT_TAG_MORX HB_TAG('m','o','r','x')


namespace AAT {

using namespace OT;


struct RearrangementSubtable
{
  struct driver_context_t
  {
    static const bool in_place = true;
    enum Flags {
      MarkFirst		= 0x8000,	/* If set, make the current glyph the first
					 * glyph to be rearranged. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph
					 * before going to the new state. This means
					 * that the glyph index doesn't change, even
					 * if the glyph at that index has changed. */
      MarkLast		= 0x2000,	/* If set, make the current glyph the last
					 * glyph to be rearranged. */
      Reserved		= 0x1FF0,	/* These bits are reserved and should be set to 0. */
      Verb		= 0x000F,	/* The type of rearrangement specified. */
    };

    inline driver_context_t (const RearrangementSubtable *table) :
	ret (false),
	start (0), end (0),
	last_zero_before_start (0) {}

    inline void transition (StateTableDriver<void> *driver,
			    const Entry<void> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;
      unsigned int flags = entry->flags;

      if (flags & MarkFirst)
      {
	start = buffer->idx;
	last_zero_before_start = driver->last_zero;
      }

      if (flags & MarkLast)
	end = MIN (buffer->idx + 1, buffer->len);

      if ((flags & Verb) && start < end)
      {
	/* The following map has two nibbles, for start-side
	 * and end-side. Values of 0,1,2 mean move that many
	 * to the other side. Value of 3 means move 2 and
	 * flip them. */
	const unsigned char map[16] =
	{
	  0x00,	/* 0	no change */
	  0x10,	/* 1	Ax => xA */
	  0x01,	/* 2	xD => Dx */
	  0x11,	/* 3	AxD => DxA */
	  0x20,	/* 4	ABx => xAB */
	  0x30,	/* 5	ABx => xBA */
	  0x02,	/* 6	xCD => CDx */
	  0x03,	/* 7	xCD => DCx */
	  0x12,	/* 8	AxCD => CDxA */
	  0x13,	/* 9	AxCD => DCxA */
	  0x21,	/* 10	ABxD => DxAB */
	  0x31,	/* 11	ABxD => DxBA */
	  0x22,	/* 12	ABxCD => CDxAB */
	  0x32,	/* 13	ABxCD => CDxBA */
	  0x23,	/* 14	ABxCD => DCxAB */
	  0x33,	/* 15	ABxCD => DCxBA */
	};

	unsigned int m = map[flags & Verb];
	unsigned int l = MIN<unsigned int> (2, m >> 4);
	unsigned int r = MIN<unsigned int> (2, m & 0x0F);
	bool reverse_l = 3 == (m >> 4);
	bool reverse_r = 3 == (m & 0x0F);

	if (end - start >= l + r)
	{
	  buffer->unsafe_to_break (last_zero_before_start, MIN (buffer->idx + 1, buffer->len));
	  buffer->merge_clusters (start, end);

	  hb_glyph_info_t *info = buffer->info;
	  hb_glyph_info_t buf[4];

	  memcpy (buf, info + start, l * sizeof (buf[0]));
	  memcpy (buf + 2, info + end - r, r * sizeof (buf[0]));

	  if (l != r)
	    memmove (info + start + r, info + start + l, (end - start - l - r) * sizeof (buf[0]));

	  memcpy (info + start, buf + 2, r * sizeof (buf[0]));
	  memcpy (info + end - l, buf, l * sizeof (buf[0]));
	  if (reverse_l)
	  {
	    buf[0] = info[end - 1];
	    info[end - 1] = info[end - 2];
	    info[end - 2] = buf[0];
	  }
	  if (reverse_r)
	  {
	    buf[0] = info[start];
	    info[start] = info[start + 1];
	    info[start + 1] = buf[0];
	  }
	}
      }
    }

    public:
    bool ret;
    private:
    unsigned int start;
    unsigned int end;
    unsigned int last_zero_before_start;
  };

  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this);

    StateTableDriver<void> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (machine.sanitize (c));
  }

  protected:
  StateTable<void>	machine;
  public:
  DEFINE_SIZE_STATIC (16);
};

struct ContextualSubtable
{
  struct EntryData
  {
    HBUINT16	markIndex;	/* Index of the substitution table for the
				 * marked glyph (use 0xFFFF for none). */
    HBUINT16	currentIndex;	/* Index of the substitution table for the
				 * current glyph (use 0xFFFF for none). */
    public:
    DEFINE_SIZE_STATIC (4);
  };

  struct driver_context_t
  {
    static const bool in_place = true;
    enum Flags {
      SetMark		= 0x8000,	/* If set, make the current glyph the marked glyph. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph before
					 * going to the new state. */
      Reserved		= 0x3FFF,	/* These bits are reserved and should be set to 0. */
    };

    inline driver_context_t (const ContextualSubtable *table) :
	ret (false),
	mark (0),
	last_zero_before_mark (0),
	subs (table+table->substitutionTables) {}

    inline void transition (StateTableDriver<EntryData> *driver,
			    const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;

      if (entry->flags & SetMark)
      {
	mark = buffer->idx;
	last_zero_before_mark = driver->last_zero;
      }

      if (entry->data.markIndex != 0xFFFF)
      {
	const Lookup<GlyphID> &lookup = subs[entry->data.markIndex];
	hb_glyph_info_t *info = buffer->info;
	const GlyphID *replacement = lookup.get_value (info[mark].codepoint, driver->num_glyphs);
	if (replacement)
	{
	  buffer->unsafe_to_break (last_zero_before_mark, MIN (buffer->idx + 1, buffer->len));
	  info[mark].codepoint = *replacement;
	  ret = true;
	}
      }
      if (entry->data.currentIndex != 0xFFFF)
      {
	const Lookup<GlyphID> &lookup = subs[entry->data.currentIndex];
	hb_glyph_info_t *info = buffer->info;
	const GlyphID *replacement = lookup.get_value (info[buffer->idx].codepoint, driver->num_glyphs);
	if (replacement)
	{
	  buffer->unsafe_to_break (driver->last_zero, MIN (buffer->idx + 1, buffer->len));
	  info[buffer->idx].codepoint = *replacement;
	  ret = true;
	}
      }
    }

    public:
    bool ret;
    private:
    unsigned int mark;
    unsigned int last_zero_before_mark;
    const UnsizedOffsetListOf<Lookup<GlyphID>, HBUINT32> &subs;
  };

  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this);

    StateTableDriver<EntryData> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);

    unsigned int num_entries;
    if (unlikely (!machine.sanitize (c, &num_entries))) return false;

    unsigned int num_lookups = 0;

    const Entry<EntryData> *entries = machine.get_entries ();
    for (unsigned int i = 0; i < num_entries; i++)
    {
      const EntryData &data = entries[i].data;

      num_lookups = MAX<unsigned int> (num_lookups, 1 + data.markIndex);
      num_lookups = MAX<unsigned int> (num_lookups, 1 + data.currentIndex);
    }

    return_trace (substitutionTables.sanitize (c, this, num_lookups));
  }

  protected:
  StateTable<EntryData>	machine;
  OffsetTo<UnsizedOffsetListOf<Lookup<GlyphID>, HBUINT32>, HBUINT32>
			substitutionTables;
  public:
  DEFINE_SIZE_STATIC (20);
};

struct LigatureSubtable
{
  struct EntryData
  {
    HBUINT16	ligActionIndex;	/* Index to the first ligActionTable entry
				 * for processing this group, if indicated
				 * by the flags. */
    public:
    DEFINE_SIZE_STATIC (2);
  };

  struct driver_context_t
  {
    static const bool in_place = false;
    enum Flags {
      SetComponent	= 0x8000,	/* Push this glyph onto the component stack for
					 * eventual processing. */
      DontAdvance	= 0x4000,	/* Leave the glyph pointer at this glyph for the
					   next iteration. */
      PerformAction	= 0x2000,	/* Use the ligActionIndex to process a ligature
					 * group. */
      Reserved		= 0x1FFF,	/* These bits are reserved and should be set to 0. */
    };

    inline driver_context_t (const LigatureSubtable *table) :
	ret (false) {}

    inline void transition (StateTableDriver<EntryData> *driver,
			    const Entry<EntryData> *entry)
    {
      hb_buffer_t *buffer = driver->buffer;

      /* TODO */
    }

    public:
    bool ret;
    private:
  };

  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this);

    StateTableDriver<EntryData> driver (machine, c->buffer, c->face);
    driver.drive (&dc);

    return_trace (dc.ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* The main sanitization is done at run-time. */
    return machine.sanitize (c);
    return_trace (true);
  }

  protected:
  StateTable<EntryData>	machine;
  OffsetTo<UnsizedArrayOf<HBUINT32>, HBUINT32>
		ligAction;	/* Offset to the ligature action table. */
  OffsetTo<UnsizedArrayOf<HBUINT16>, HBUINT32>
		component;	/* Offset to the component table. */
  OffsetTo<UnsizedArrayOf<GlyphID>, HBUINT32>
		ligature;	/* Offset to the actual ligature lists. */
  public:
  DEFINE_SIZE_STATIC (28);
};

struct NoncontextualSubtable
{
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    bool ret = false;
    unsigned int num_glyphs = c->face->get_num_glyphs ();

    hb_glyph_info_t *info = c->buffer->info;
    unsigned int count = c->buffer->len;
    for (unsigned int i = 0; i < count; i++)
    {
      const GlyphID *replacement = substitute.get_value (info[i].codepoint, num_glyphs);
      if (replacement)
      {
	info[i].codepoint = *replacement;
	ret = true;
      }
    }

    return_trace (ret);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (substitute.sanitize (c));
  }

  protected:
  Lookup<GlyphID>	substitute;
  public:
  DEFINE_SIZE_MIN (2);
};

struct InsertionSubtable
{
  inline bool apply (hb_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    /* TODO */
    return_trace (false);
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* TODO */
    return_trace (true);
  }
};


struct Feature
{
  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16	featureType;	/* The type of feature. */
  HBUINT16	featureSetting;	/* The feature's setting (aka selector). */
  HBUINT32	enableFlags;	/* Flags for the settings that this feature
				 * and setting enables. */
  HBUINT32	disableFlags;	/* Complement of flags for the settings that this
				 * feature and setting disable. */

  public:
  DEFINE_SIZE_STATIC (12);
};


struct ChainSubtable
{
  friend struct Chain;

  inline unsigned int get_size (void) const { return length; }
  inline unsigned int get_type (void) const { return coverage & 0xFF; }

  enum Type {
    Rearrangement	= 0,
    Contextual		= 1,
    Ligature		= 2,
    Noncontextual	= 4,
    Insertion		= 5
  };

  inline void apply (hb_apply_context_t *c, const char *end) const
  {
    dispatch (c, end);
  }

  template <typename context_t>
  inline typename context_t::return_t dispatch (context_t *c, const char *end) const
  {
    unsigned int subtable_type = get_type ();
    TRACE_DISPATCH (this, subtable_type);
    switch (subtable_type) {
    case Rearrangement:		return_trace (c->dispatch (u.rearrangement));
    case Contextual:		return_trace (c->dispatch (u.contextual));
    case Ligature:		return_trace (c->dispatch (u.ligature));
    case Noncontextual:		return_trace (c->dispatch (u.noncontextual));
    case Insertion:		return_trace (c->dispatch (u.insertion));
    default:			return_trace (c->default_return_value ());
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!length.sanitize (c) ||
	length < min_size ||
	!c->check_range (this, length))
      return_trace (false);

    return_trace (dispatch (c, c->end));
  }

  protected:
  HBUINT32	length;		/* Total subtable length, including this header. */
  HBUINT32	coverage;	/* Coverage flags and subtable type. */
  HBUINT32	subFeatureFlags;/* The 32-bit mask identifying which subtable this is. */
  union {
  RearrangementSubtable		rearrangement;
  ContextualSubtable		contextual;
  LigatureSubtable		ligature;
  NoncontextualSubtable		noncontextual;
  InsertionSubtable		insertion;
  } u;
  public:
  DEFINE_SIZE_MIN (12);
};

struct Chain
{
  inline void apply (hb_apply_context_t *c, const char *end) const
  {
    const ChainSubtable *subtable = &StructAtOffset<ChainSubtable> (featureZ, featureZ[0].static_size * featureCount);
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      subtable->apply (c, end);
      subtable = &StructAfter<ChainSubtable> (*subtable);
    }
  }

  inline unsigned int get_size (void) const { return length; }

  inline bool sanitize (hb_sanitize_context_t *c, unsigned int major) const
  {
    TRACE_SANITIZE (this);
    if (!length.sanitize (c) ||
	length < min_size ||
	!c->check_range (this, length))
      return_trace (false);

    if (!c->check_array (featureZ, featureZ[0].static_size, featureCount))
      return_trace (false);

    const ChainSubtable *subtable = &StructAtOffset<ChainSubtable> (featureZ, featureZ[0].static_size * featureCount);
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      if (!subtable->sanitize (c))
	return_trace (false);
      subtable = &StructAfter<ChainSubtable> (*subtable);
    }

    return_trace (true);
  }

  protected:
  HBUINT32	defaultFlags;	/* The default specification for subtables. */
  HBUINT32	length;		/* Total byte count, including this header. */
  HBUINT32	featureCount;	/* Number of feature subtable entries. */
  HBUINT32	subtableCount;	/* The number of subtables in the chain. */

  Feature	featureZ[VAR];	/* Features. */
  ChainSubtable	subtableX[VAR];	/* Subtables. */
  // subtableGlyphCoverageArray if major == 3

  public:
  DEFINE_SIZE_MIN (16);
};


/*
 * The 'mort'/'morx' Tables
 */

struct morx
{
  static const hb_tag_t tableTag = HB_AAT_TAG_MORX;

  inline void apply (hb_apply_context_t *c, unsigned int length) const
  {
    const char *end = (const char *) this + length;
    const Chain *chain = chains;
    unsigned int count = chainCount;
    for (unsigned int i = 0; i < count; i++)
    {
      chain->apply (c, end);
      chain = &StructAfter<Chain> (*chain);
    }
  }

  inline bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!version.sanitize (c) ||
	(version.major >> (sizeof (HBUINT32) == 4 ? 1 : 0)) != 1 ||
	!chainCount.sanitize (c))
      return_trace (false);

    const Chain *chain = chains;
    unsigned int count = chainCount;
    for (unsigned int i = 0; i < count; i++)
    {
      if (!chain->sanitize (c, version.major))
	return_trace (false);
      chain = &StructAfter<Chain> (*chain);
    }

    return_trace (true);
  }

  protected:
  FixedVersion<>version;	/* Version number of the glyph metamorphosis table.
				 * 1 for mort, 2 or 3 for morx. */
  HBUINT32	chainCount;	/* Number of metamorphosis chains contained in this
				 * table. */
  Chain		chains[VAR];	/* Chains. */

  public:
  DEFINE_SIZE_MIN (8);
};

} /* namespace AAT */


#endif /* HB_AAT_LAYOUT_MORX_TABLE_HH */
