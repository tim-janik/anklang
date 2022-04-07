// This Source Code Form is licensed MPL-2.0: http://mozilla.org/MPL/2.0
#ifndef __ASE_EVENTLIST_HH__
#define __ASE_EVENTLIST_HH__

#include <ase/utils.hh>
#include <any>

namespace Ase {

/// Container for a sorted array of opaque `Event` structures with binary lookup.
template<class Event, class CompareOrder>
struct OrderedEventList : std::vector<Event> {
  using Base = std::vector<Event>;
  using ConstP = std::shared_ptr<const OrderedEventList>;
  explicit     OrderedEventList (const std::vector<Event> &ve);
  const Event* lookup           (const Event &event) const;
  const Event* lookup_after     (const Event &event) const;
private:
  CompareOrder compare_;
};

/// Maintain an array of unique `Event` structures with change notification.
template<class Event, class Compare>
class EventList final {
  using EventVector = std::vector<Event>;
public:
  using Notify = std::function<void (const Event &event, int mod)>;
  using CIter = typename EventVector::const_iterator;
  explicit     EventList      (const Notify &n = {}, const Compare &c = {});
  bool         insert         (const Event &event); /// Insert or replace `event`, notifies.
  bool         remove         (const Event &event); /// Return true if `event` was removed, notifies.
  const Event* lookup         (const Event &event) const; /// Return pointer to matching `event` or nullptr.
  const Event* lookup_after   (const Event &event) const; /// Return pointer to element that is >= `event` or nullptr.
  const Event* first          () const; /// Return first element or nullptr.
  const Event* last           () const; /// Return last element or nullptr.
  size_t       size           () const; /// Return the numberof elements.
  void         clear_silently ();       /// Clear list without notification.
  template<class OrderedEventList> typename OrderedEventList::ConstP
  inline       ordered_events ();       /// Create a read-only copy of this EventList (possibly cached).
  CIter        begin          () const { return events_.begin(); } /// Const iterator that points to the first element.
  CIter        end            () const { return events_.end(); }   /// Const iterator that points one past the last element.
private:
  EventVector        events_;
  Compare            compare_;
  Notify             notify_;
  std::any           ordered_;
  static_assert (std::is_signed<decltype (std::declval<Compare>() (std::declval<Event>(), std::declval<Event>()))>::value, "REQUIRE: int Compare (const&, const&);");
  void               uncache      ();
  static void        nop          (const Event&, int) {}
};

// == Implementation Details ==
template<class Event, class Compare> inline
EventList<Event,Compare>::EventList (const Notify &n, const Compare &c) :
  compare_ (c), notify_ (n)
{
  if (!notify_)
    notify_ = nop;
}

template<class Event, class Compare> inline void
EventList<Event,Compare>::clear_silently ()
{
  events_.clear();
  ordered_.reset();
}

template<class Event, class Compare> inline void
EventList<Event,Compare>::uncache ()
{
  ordered_.reset();
}

template<class Event, class Compare> inline bool
EventList<Event,Compare>::insert (const Event &event)
{
  uncache();
  if (events_.size() && compare_ (event, events_.back()) > 0)
    {
      events_.push_back (event);
      notify_ (event, +1);      // notify insertion
      return true;              // O(1) fast path for append
    }
  auto insmatch = Aux::binary_lookup_insertion_pos (events_.begin(), events_.end(), compare_, event);
  auto it = insmatch.first;
  if (insmatch.second == true)  // exact match
    {
      *it = event;
      notify_ (event, 0);       // notify change
      return false;
    }
  else
    {
      events_.insert (it, event);
      notify_ (event, +1);      // notify insertion
      return true;
    }
}

template<class Event, class Compare> inline bool
EventList<Event,Compare>::remove (const Event &event)
{
  uncache();
  const int cmp = events_.empty() ? +1 : compare_ (event, events_.back());
  if (cmp == 0)
    {
      events_.pop_back();       // O(1) fast path for tail removal
      notify_ (event, -1);      // notify removal
      return true;              // found and removed
    }
  if (cmp < 0)
    {
      auto it = Aux::binary_lookup (events_.begin(), events_.end() - 1, compare_, event);
      if (it != events_.end())
        {
          events_.erase (it);
          notify_ (event, -1);  // notify removal
          return true;          // found and removed
        }
    }
  return false;                 // none removed
}

template<class Event, class Compare> inline const Event*
EventList<Event,Compare>::first() const
{
  return events_.empty() ? nullptr : &events_.front();
}

template<class Event, class Compare> inline const Event*
EventList<Event,Compare>::last() const
{
  return events_.empty() ? nullptr : &events_.back();
}

template<class Event, class Compare> inline size_t
EventList<Event,Compare>::size () const
{
  return events_.size();
}

template<class Event, class Compare> inline const Event*
EventList<Event,Compare>::lookup (const Event &event) const
{
  auto it = Aux::binary_lookup (events_.begin(), events_.end(), compare_, event);
  return it != events_.end() ? &*it : nullptr;
}

template<class Event, class Compare> inline const Event*
EventList<Event,Compare>::lookup_after (const Event &event) const
{
  auto it = Aux::binary_lookup_insertion_pos (events_.begin(), events_.end(), compare_, event).first;
  return it != events_.end() ? &*it : nullptr;
}

template<class Event, class Compare>
template<class OrderedEventList> inline typename OrderedEventList::ConstP
EventList<Event,Compare>::ordered_events ()
{
  using OrderedEventListP = typename OrderedEventList::ConstP;
  OrderedEventListP *oepp = std::any_cast<OrderedEventListP> (&ordered_);
  if (!oepp)
    {
      ordered_ = std::make_shared<const OrderedEventList> (events_);
      oepp = std::any_cast<OrderedEventListP> (&ordered_);
      ASE_ASSERT_RETURN (oepp, nullptr);
    }
  return *oepp;
}

template<class Event, class CompareOrder>
OrderedEventList<Event,CompareOrder>::OrderedEventList (const std::vector<Event> &ve) :
  Base (ve)
{
  auto lesser = [this] (const Event &a, const Event &b) {
    return compare_ (a, b) < 0;
  };
  std::stable_sort (this->begin(), this->end(), lesser);
}

template<class Event, class CompareOrder> inline const Event*
OrderedEventList<Event,CompareOrder>::lookup (const Event &event) const
{
  auto it = Aux::binary_lookup (this->begin(), this->end(), compare_, event);
  return it != this->end() ? &*it : nullptr;
}

template<class Event, class CompareOrder> inline const Event*
OrderedEventList<Event,CompareOrder>::lookup_after (const Event &event) const
{
  auto it = Aux::binary_lookup_insertion_pos (this->begin(), this->end(), compare_, event).first;
  return it != this->end() ? &*it : nullptr;
}

} // Ase

#endif // __ASE_EVENTLIST_HH__
