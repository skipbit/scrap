#include "repository/driver/LibGitRepository.h"

#include <git2.h>
#include <git2/annotated_commit.h>
#include <git2/merge.h>
#include <iostream>

namespace scrap::repository::libgit {

/**
 * @brief The internal class for the repository class.
 */
class Remote {
public:
    Remote(git_repository* repository, const std::string& name) {
        if (GIT_OK != git_remote_lookup(&remote_, repository, name.c_str())) {
            throw std::runtime_error("Failed to lookup remote");
        }
    }

    ~Remote() {
        if (remote_) {
            git_remote_free(remote_);
        }
    }

    void fetch() {
        if (GIT_OK != git_remote_fetch(remote_, nullptr, nullptr, nullptr)) {
            throw std::runtime_error("Failed to fetch remote");
        }
    }

    git_remote* remote_ = nullptr;
};

/**
 * @brief The internal class for the repository class.
 */
class Commit {
public:
    Commit(git_repository* repository, const git_oid* oid) {
        if (GIT_OK != git_commit_lookup(&commit_, repository, oid)) {
            throw std::runtime_error("Failed to lookup commit");
        }
    }

    ~Commit() {
        if (commit_) {
            git_commit_free(commit_);
        }
    }

    const git_oid* oid() const {
        return git_commit_id(commit_);
    }

    git_commit* commit_ = nullptr;
};

/**
 * @brief The internal class for the repository class.
 */
class Reference {
public:
    Reference(git_repository* repository, const std::string& name) {
        if (GIT_OK != git_reference_lookup(&reference_, repository, name.c_str())) {
            throw std::runtime_error("Failed to lookup reference");
        }
    }

    ~Reference() {
        if (reference_) {
            git_reference_free(reference_);
        }
    }

    const git_oid* oid() const {
        return git_reference_target(reference_);
    }

    const char* name() const {
        return git_reference_name(reference_);
    }

    void setTarget(const Commit& c, const std::string& message) {
        if (GIT_OK != git_reference_set_target(&reference_, reference_, c.oid(), message.c_str())) {
            throw std::runtime_error("Failed to set reference target");
        }
    }

    git_reference* reference_ = nullptr;
};

class Annotation {
public:
    Annotation(git_repository* repository, const git_reference* reference) {
        if (GIT_OK != git_annotated_commit_from_ref(&annotation_, repository, reference)) {
            throw std::runtime_error("Failed to create annotated commit");
        }
    }

    ~Annotation() {
        if (annotation_) {
            git_annotated_commit_free(annotation_);
        }
    }

    operator const git_annotated_commit*() const {
        return annotation_;
    }

    git_annotated_commit* annotation_ = nullptr;
};

/**
 * @brief The internal class for the repository class.
 */
class Repository::Internal {
public:
    // --
    Internal(const std::filesystem::path& path) {
        git_repository_open(&repository, path.c_str());
    }
    // --
    Internal(const std::string& url, const std::filesystem::path& path) {
        git_clone_options options;
        if (GIT_OK != git_clone_options_init(&options, GIT_CLONE_OPTIONS_VERSION)) {
            throw std::runtime_error("Failed to initialize clone options");
        }
        git_clone(&repository, url.c_str(), path.c_str(), &options);
    }
    // --
    ~Internal() {
        if (repository) {
            git_repository_free(repository);
        }
    }

    // --
    void update(const std::string& r = "origin", const std::string& b = "main") {
        try {
            fetch(r);
            Commit remoteCommit(repository, Reference(repository, "refs/remotes/" + r + "/" + b).oid());
            Reference localHead(repository, "refs/heads/" + b);
            merge(remoteCommit, localHead);
        } catch (const std::exception& e) {
            std::cerr << "update: " << e.what() << std::endl;
        }
    }

    // --
    void fetch(const std::string& r = "origin") {
        try {
            Remote(repository, r).fetch();
        } catch (const std::exception& e) {
            std::cerr << "fetch: " << e.what() << std::endl;
        }
    }

    // --
    void merge(Commit& co, Reference& ref) {
        try {
            git_merge_analysis_t analysis;
            git_merge_preference_t preference;

            Annotation ac(repository, ref.reference_);
            const git_annotated_commit* annotations[] = { ac };

            if (GIT_OK != git_merge_analysis(&analysis, &preference, repository, annotations, 1)) {
                throw std::runtime_error("Failed to analyze merge");
            }
            if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
                return;
            }
            else if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
                ref.setTarget(co, "Fast-forward");
                setHeadToRef(ref);
                checkoutHead();
            }
            else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
                git_merge_options options;
                if (GIT_OK != git_merge_init_options(&options, GIT_MERGE_OPTIONS_VERSION)) {
                    throw std::runtime_error("Failed to initialize merge options");
                }
                if (GIT_OK != git_merge(repository, annotations, 1, &options, nullptr)) {
                    throw std::runtime_error("Failed to merge");
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "merge: " << e.what() << std::endl;
        }
    }

    // --
    void setHeadToRef(const Reference& l) {
        if (GIT_OK != git_repository_set_head(repository, l.name())) {
            throw std::runtime_error("Failed to set repository head");
        }
    }

    // --
    void checkoutHead() {
        git_checkout_options options;
        if (GIT_OK != git_checkout_options_init(&options, GIT_CHECKOUT_OPTIONS_VERSION)) {
            throw std::runtime_error("Failed to initialize checkout options");
        }
        if (GIT_OK != git_checkout_head(repository, &options)) {
            throw std::runtime_error("Failed to checkout head");
        }
    }

    // --
    git_repository* repository = nullptr;
};

Repository::Repository(const std::filesystem::path& p)
    : impl_(std::make_unique<Internal>(p))
{
}

Repository::Repository(const std::string& url, const std::filesystem::path& p)
    : impl_(std::make_unique<Internal>(url, p))
{
}

Repository::~Repository() = default;

void Repository::update(const std::string& remote, const std::string& branch)
{
    impl_->update(remote, branch);
}

}
