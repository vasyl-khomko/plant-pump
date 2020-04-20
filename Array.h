template<typename T, int n> struct Array {
  int size = n;
  T elements[n];

  void addFirst(T e) {
    for (int i = n - 2; i >= 0; i--) {
      elements[i + 1] = elements[i];
    }

    elements[0] = e;
  }
};