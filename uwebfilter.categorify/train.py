from sklearn.calibration import CalibratedClassifierCV
from sklearn.linear_model import RidgeClassifier
from sklearn.feature_extraction.text import TfidfVectorizer

import pandas as pd


def train(dataset, verbose=False):
    """
    given a dataset, train a machine learning model

    dataset should have 2 columns
        category:    name of category
        words:       list of words

        example
            category,words
            Travel,hotel stay trip travel visit book comfortable
            Streaming,watch movie video stream like comment
            Fitness,gym workout fitness health body building

    returns the model (along with utility functions)
    """
    if verbose:
        print(f":> params(dataset=`{dataset}`)")

    df = pd.read_csv(dataset)
    df_category_unique = df["category"].unique()
    if verbose:
        print(f':> categories: [{", ".join(df_category_unique)}]')

    # create a new column 'category_id' with encoded categories
    df["category_id"] = df["category"].factorize()[0]

    # dictionary for translating between prediction and category
    id_to_category = dict(
        df[["category_id", "category"]].drop_duplicates().values
    )

    X = df["words"]  # collection of text
    C = df["category_id"]  # encoded categories

    tfidf = TfidfVectorizer(
        sublinear_tf=True,
        min_df=5,
        ngram_range=(1, 2),
        stop_words="english",
    )

    if verbose:
        print(":> fitting vectorizer")

    fitted_vectorizer = tfidf.fit(X)
    tfidf_vectorizer_vectors = fitted_vectorizer.transform(X)

    if verbose:
        print(":> calibrating model")

    model_ridge = RidgeClassifier().fit(tfidf_vectorizer_vectors, C)
    model_calibrated = CalibratedClassifierCV(
        estimator=model_ridge,
        n_jobs=-1,
    ).fit(tfidf_vectorizer_vectors, C)

    utils = {
        "model": model_calibrated,
        "id_to_category": id_to_category,
        "fitted_vectorizer": fitted_vectorizer,
        "df_category_unique": df_category_unique,
    }

    return utils


if __name__ == "__main__":
    import pickle
    import argparse

    cmdline = argparse.ArgumentParser(
        description="website classification model trainer",
    )

    cmdline.add_argument(
        "dataset",
        help="dataset to be trained on",
    )
    cmdline.add_argument(
        "--name",
        type=str,
        required=True,
        help="name of the output model",
    )
    cmdline.add_argument(
        "--verbose",
        action="store_true",
        help="enable verbose mode",
    )

    args = cmdline.parse_args()

    print("training model")

    utils = train(args.dataset, verbose=args.verbose)

    outpickle = f"{args.name}.pickle"
    with open(f"models/{outpickle}", "wb") as handle:
        pickle.dump(utils, handle, protocol=pickle.HIGHEST_PROTOCOL)

    print(f"model saved to {outpickle}")
