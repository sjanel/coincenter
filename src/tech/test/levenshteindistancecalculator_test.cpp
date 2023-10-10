#include "levenshteindistancecalculator.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(LevenshteinDistanceCalculator, CornerCases) {
  LevenshteinDistanceCalculator calc;

  EXPECT_EQ(calc("", "tata"), 4);
  EXPECT_EQ(calc("tutu", ""), 4);
}

TEST(LevenshteinDistanceCalculator, SimpleCases) {
  LevenshteinDistanceCalculator calc;

  EXPECT_EQ(calc("horse", "ros"), 3);
  EXPECT_EQ(calc("intention", "execution"), 5);
  EXPECT_EQ(calc("niche", "chien"), 4);
}

TEST(LevenshteinDistanceCalculator, TypicalCases) {
  LevenshteinDistanceCalculator calc;

  EXPECT_EQ(calc("--orderbook", "orderbook"), 2);
  EXPECT_EQ(calc("--timeout-match", "--timeot-match"), 1);
  EXPECT_EQ(calc("--no-multi-trade", "--no-mukti-trade"), 1);
  EXPECT_EQ(calc("--updt-price", "--update-price"), 2);
}

TEST(LevenshteinDistanceCalculator, ExtremeCases) {
  LevenshteinDistanceCalculator calc;

  EXPECT_EQ(
      calc(
          "Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the "
          "industry's standard dummy text ever since the 1500s, when an unknown printer took a galley of type and "
          "scrambled it to make a type specimen book. It has survived not only five centuries, but also the leap into "
          "electronic typesetting, remaining essentially unchanged. It was popularised in the 1960s with the release "
          "of Letraset sheets containing Lorem Ipsum passages, and more recently with desktop publishing software "
          "like Aldus PageMaker including versions of Lorem Ipsum.",
          "Lorem Ipsum is simply dummy text of the printing and typesetting industry. Lorem Ipsum has been the "
          "industrj's standard dummytext ever since the 1500s, when an unknown printer took a galley of type and "
          "scrambled iT to make a type specimen book. I has survived not only five centuroes, but also the leap into "
          "electronic typesetting, i remaining essentially unchanged. It was popularised in the 1960s with the release "
          "of Letraset sheets; containing Lorem Ipsum passages, and more recently with desktogp publishing software "
          "like Aldus PageMaker including versions of Lorem Ipsum."),
      9);
}
}  // namespace cct